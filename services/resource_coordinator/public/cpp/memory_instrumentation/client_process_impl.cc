// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/client_process_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/flat_map.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

namespace memory_instrumentation {

// static
void ClientProcessImpl::CreateInstance(
    mojo::PendingReceiver<mojom::ClientProcess> receiver,
    mojo::PendingRemote<mojom::Coordinator> coordinator,
    bool is_browser_process) {
  static ClientProcessImpl* instance = nullptr;
  if (!instance) {
    instance = new ClientProcessImpl(
        std::move(receiver), std::move(coordinator), is_browser_process,
        /*initialize_memory_instrumentation=*/true);
  } else {
    NOTREACHED();
  }
}

ClientProcessImpl::ClientProcessImpl(
    mojo::PendingReceiver<mojom::ClientProcess> receiver,
    mojo::PendingRemote<mojom::Coordinator> coordinator,
    bool is_browser_process,
    bool initialize_memory_instrumentation)
    : receiver_(this, std::move(receiver)) {
  if (initialize_memory_instrumentation) {
    // Initialize the public-facing MemoryInstrumentation helper.
    MemoryInstrumentation::CreateInstance(std::move(coordinator));
  } else {
    coordinator_.Bind(std::move(coordinator));
  }

  task_runner_ = base::ThreadTaskRunnerHandle::Get();

  // TODO(primiano): this is a temporary workaround to tell the
  // base::MemoryDumpManager that it is special and should coordinate periodic
  // dumps for tracing. Remove this once the periodic dump scheduler is moved
  // from base to the service. MDM should not care about being the coordinator.
  base::trace_event::MemoryDumpManager::GetInstance()->Initialize(
      base::BindRepeating(
          &ClientProcessImpl::RequestGlobalMemoryDump_NoCallback,
          base::Unretained(this)),
      is_browser_process);

  tracing_observer_ = std::make_unique<TracingObserver>(
      base::trace_event::TraceLog::GetInstance(),
      base::trace_event::MemoryDumpManager::GetInstance());
}

ClientProcessImpl::~ClientProcessImpl() = default;

void ClientProcessImpl::RequestChromeMemoryDump(
    const base::trace_event::MemoryDumpRequestArgs& args,
    RequestChromeMemoryDumpCallback callback) {
  DCHECK(!callback.is_null());
  most_recent_chrome_memory_dump_guid_ = args.dump_guid;
  auto it_and_inserted =
      pending_chrome_callbacks_.emplace(args.dump_guid, std::move(callback));
  DCHECK(it_and_inserted.second) << "Duplicated request id " << args.dump_guid;
  base::trace_event::MemoryDumpManager::GetInstance()->CreateProcessDump(
      args, base::BindOnce(&ClientProcessImpl::OnChromeMemoryDumpDone,
                           base::Unretained(this)));
}

void ClientProcessImpl::OnChromeMemoryDumpDone(
    bool success,
    uint64_t dump_guid,
    std::unique_ptr<base::trace_event::ProcessMemoryDump> process_memory_dump) {
  DCHECK(success || !process_memory_dump);

  auto callback_it = pending_chrome_callbacks_.find(dump_guid);
  DCHECK(callback_it != pending_chrome_callbacks_.end());

  auto callback = std::move(callback_it->second);
  pending_chrome_callbacks_.erase(callback_it);

  auto it = delayed_os_memory_dump_callbacks_.find(dump_guid);
  if (it != delayed_os_memory_dump_callbacks_.end()) {
    for (auto& args : it->second) {
      PerformOSMemoryDump(std::move(args));
    }
    delayed_os_memory_dump_callbacks_.erase(it);
  }

  if (!process_memory_dump) {
    std::move(callback).Run(false, dump_guid, nullptr);
    return;
  }
  std::move(callback).Run(success, dump_guid, std::move(process_memory_dump));
}

void ClientProcessImpl::RequestGlobalMemoryDump_NoCallback(
    base::trace_event::MemoryDumpType dump_type,
    base::trace_event::MemoryDumpLevelOfDetail level_of_detail) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ClientProcessImpl::RequestGlobalMemoryDump_NoCallback,
                       base::Unretained(this), dump_type, level_of_detail));
    return;
  }

  // NOTE: If this ClientProcessImpl was responsible for initializing the
  // global MemoryInstrumentation object, its Coordinator pipe was passed along
  // to that object, and |coordinator_| is unbound.
  mojom::Coordinator* coordinator = nullptr;
  if (coordinator_)
    coordinator = coordinator_.get();
  else
    coordinator = MemoryInstrumentation::GetInstance()->GetCoordinator();
  coordinator->RequestGlobalMemoryDumpAndAppendToTrace(
      dump_type, level_of_detail,
      base::trace_event::MemoryDumpDeterminism::NONE,
      mojom::Coordinator::RequestGlobalMemoryDumpAndAppendToTraceCallback());
}

void ClientProcessImpl::RequestOSMemoryDump(
    mojom::MemoryMapOption mmap_option,
    const std::vector<base::ProcessId>& pids,
    RequestOSMemoryDumpCallback callback) {
  OSMemoryDumpArgs args;
  args.mmap_option = mmap_option;
  args.pids = pids;
  args.callback = std::move(callback);

#if defined(OS_MACOSX)
  // If the most recent chrome memory dump hasn't finished, wait for that to
  // finish.
  if (most_recent_chrome_memory_dump_guid_.has_value()) {
    uint64_t guid = most_recent_chrome_memory_dump_guid_.value();
    auto it = pending_chrome_callbacks_.find(guid);
    if (it != pending_chrome_callbacks_.end()) {
      delayed_os_memory_dump_callbacks_[guid].push_back(std::move(args));
      return;
    }
  }
#endif
  PerformOSMemoryDump(std::move(args));
}

void ClientProcessImpl::PerformOSMemoryDump(OSMemoryDumpArgs args) {
  bool global_success = true;
  base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr> results;
  for (const base::ProcessId& pid : args.pids) {
    mojom::RawOSMemDumpPtr result = mojom::RawOSMemDump::New();
    result->platform_private_footprint = mojom::PlatformPrivateFootprint::New();
    bool success = OSMetrics::FillOSMemoryDump(pid, result.get());
    if (args.mmap_option != mojom::MemoryMapOption::NONE) {
      success = success && OSMetrics::FillProcessMemoryMaps(
                               pid, args.mmap_option, result.get());
    }
    if (success)
      results[pid] = std::move(result);
    global_success = global_success && success;
  }
  std::move(args.callback).Run(global_success, std::move(results));
}

ClientProcessImpl::OSMemoryDumpArgs::OSMemoryDumpArgs() = default;
ClientProcessImpl::OSMemoryDumpArgs::OSMemoryDumpArgs(OSMemoryDumpArgs&&) =
    default;
ClientProcessImpl::OSMemoryDumpArgs::~OSMemoryDumpArgs() = default;

}  // namespace memory_instrumentation
