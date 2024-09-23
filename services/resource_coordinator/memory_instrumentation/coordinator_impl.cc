// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/coordinator_impl.h"

#include <inttypes.h>
#include <stdio.h>

#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "services/resource_coordinator/memory_instrumentation/queued_request_dispatcher.h"
#include "services/resource_coordinator/memory_instrumentation/switches.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/client_process_impl.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer_proto.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/constants.mojom.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

using base::trace_event::MemoryDumpDeterminism;
using base::trace_event::MemoryDumpLevelOfDetail;
using base::trace_event::MemoryDumpType;

namespace memory_instrumentation {

namespace {

memory_instrumentation::CoordinatorImpl* g_coordinator_impl;

constexpr base::TimeDelta kHeapDumpTimeout = base::Seconds(60);

// A wrapper classes that allows a string to be exported as JSON in a trace
// event.
class StringWrapper : public base::trace_event::ConvertableToTraceFormat {
 public:
  explicit StringWrapper(std::string&& json) : json_(std::move(json)) {}

  void AppendAsTraceFormat(std::string* out) const override {
    out->append(json_);
  }

  std::string json_;
};

}  // namespace

CoordinatorImpl::CoordinatorImpl()
    : next_dump_id_(0),
      client_process_timeout_(base::Seconds(15)),
      write_proto_heap_profile_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kUseHeapProfilingProtoWriter)) {
  DCHECK(!g_coordinator_impl);
  g_coordinator_impl = this;
  base::trace_event::MemoryDumpManager::GetInstance()->set_tracing_process_id(
      mojom::kServiceTracingProcessId);
}

CoordinatorImpl::~CoordinatorImpl() {
  g_coordinator_impl = nullptr;
}

// static
CoordinatorImpl* CoordinatorImpl::GetInstance() {
  return g_coordinator_impl;
}

void CoordinatorImpl::RegisterHeapProfiler(
    mojo::PendingRemote<mojom::HeapProfiler> profiler,
    mojo::PendingReceiver<mojom::HeapProfilerHelper> helper_receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  heap_profiler_.Bind(std::move(profiler));
  heap_profiler_helper_receiver_.Bind(std::move(helper_receiver));
}

void CoordinatorImpl::RegisterClientProcess(
    mojo::PendingReceiver<mojom::Coordinator> receiver,
    mojo::PendingRemote<mojom::ClientProcess> client_process,
    mojom::ProcessType process_type,
    base::ProcessId process_id,
    const std::optional<std::string>& service_name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  mojo::Remote<mojom::ClientProcess> process(std::move(client_process));
  if (receiver.is_valid())
    coordinator_receivers_.Add(this, std::move(receiver), process_id);
  process.set_disconnect_handler(
      base::BindOnce(&CoordinatorImpl::UnregisterClientProcess,
                     base::Unretained(this), process_id));
  auto result = clients_.emplace(
      process_id, std::make_unique<ClientInfo>(std::move(process), process_type,
                                               service_name));
  DCHECK(result.second) << "Cannot register process " << process_id
                        << " with type " << static_cast<int>(process_type)
                        << ". Already registered for "
                        << static_cast<int>(
                               clients_.find(process_id)->second->process_type);
}

void CoordinatorImpl::RequestGlobalMemoryDump(
    MemoryDumpType dump_type,
    MemoryDumpLevelOfDetail level_of_detail,
    MemoryDumpDeterminism determinism,
    const std::vector<std::string>& allocator_dump_names,
    RequestGlobalMemoryDumpCallback callback) {
  // This merely strips out the |dump_guid| argument.
  auto adapter = [](RequestGlobalMemoryDumpCallback callback, bool success,
                    uint64_t, mojom::GlobalMemoryDumpPtr global_memory_dump) {
    std::move(callback).Run(success, std::move(global_memory_dump));
  };

  QueuedRequest::Args args(dump_type, level_of_detail, determinism,
                           allocator_dump_names, false /* add_to_trace */,
                           base::kNullProcessId,
                           /*memory_footprint_only=*/false);
  RequestGlobalMemoryDumpInternal(args,
                                  base::BindOnce(adapter, std::move(callback)));
}

void CoordinatorImpl::RequestGlobalMemoryDumpForPid(
    base::ProcessId pid,
    const std::vector<std::string>& allocator_dump_names,
    RequestGlobalMemoryDumpForPidCallback callback) {
  // Error out early if process id is null to avoid confusing with global
  // dump for all processes case when pid is kNullProcessId.
  if (pid == base::kNullProcessId) {
    std::move(callback).Run(false, nullptr);
    return;
  }

  // This merely strips out the |dump_guid| argument; this is not relevant
  // as we are not adding to trace.
  auto adapter = [](RequestGlobalMemoryDumpForPidCallback callback,
                    bool success, uint64_t,
                    mojom::GlobalMemoryDumpPtr global_memory_dump) {
    std::move(callback).Run(success, std::move(global_memory_dump));
  };

  QueuedRequest::Args args(
      base::trace_event::MemoryDumpType::kSummaryOnly,
      base::trace_event::MemoryDumpLevelOfDetail::kBackground,
      base::trace_event::MemoryDumpDeterminism::kNone, allocator_dump_names,
      false /* add_to_trace */, pid,
      /*memory_footprint_only=*/false);
  RequestGlobalMemoryDumpInternal(args,
                                  base::BindOnce(adapter, std::move(callback)));
}

void CoordinatorImpl::RequestPrivateMemoryFootprint(
    base::ProcessId pid,
    RequestPrivateMemoryFootprintCallback callback) {
  // This merely strips out the |dump_guid| argument; this is not relevant
  // as we are not adding to trace.
  auto adapter = [](RequestPrivateMemoryFootprintCallback callback,
                    bool success, uint64_t,
                    mojom::GlobalMemoryDumpPtr global_memory_dump) {
    std::move(callback).Run(success, std::move(global_memory_dump));
  };

  QueuedRequest::Args args(
      base::trace_event::MemoryDumpType::kSummaryOnly,
      base::trace_event::MemoryDumpLevelOfDetail::kBackground,
      base::trace_event::MemoryDumpDeterminism::kNone, {},
      false /* add_to_trace */, pid, /*memory_footprint_only=*/true);
  RequestGlobalMemoryDumpInternal(args,
                                  base::BindOnce(adapter, std::move(callback)));
}

void CoordinatorImpl::RequestGlobalMemoryDumpAndAppendToTrace(
    MemoryDumpType dump_type,
    MemoryDumpLevelOfDetail level_of_detail,
    MemoryDumpDeterminism determinism,
    RequestGlobalMemoryDumpAndAppendToTraceCallback callback) {
  // This merely strips out the |dump_ptr| argument.
  auto adapter = [](RequestGlobalMemoryDumpAndAppendToTraceCallback callback,
                    bool success, uint64_t dump_guid,
                    mojom::GlobalMemoryDumpPtr) {
    std::move(callback).Run(success, dump_guid);
  };

  QueuedRequest::Args args(dump_type, level_of_detail, determinism, {},
                           true /* add_to_trace */, base::kNullProcessId,
                           /*memory_footprint_only=*/false);
  RequestGlobalMemoryDumpInternal(args,
                                  base::BindOnce(adapter, std::move(callback)));
}

void CoordinatorImpl::GetVmRegionsForHeapProfiler(
    const std::vector<base::ProcessId>& pids,
    GetVmRegionsForHeapProfilerCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  uint64_t dump_guid = ++next_dump_id_;
  std::unique_ptr<QueuedVmRegionRequest> request =
      std::make_unique<QueuedVmRegionRequest>(dump_guid, std::move(callback));
  in_progress_vm_region_requests_[dump_guid] = std::move(request);

  std::vector<QueuedRequestDispatcher::ClientInfo> clients;
  for (const auto& entry : clients_) {
    const base::ProcessId pid = entry.first;
    clients.emplace_back(entry.second->client.get(), pid,
                         entry.second->process_type,
                         entry.second->service_name);
  }

  QueuedVmRegionRequest* request_ptr =
      in_progress_vm_region_requests_[dump_guid].get();
  auto os_callback =
      base::BindRepeating(&CoordinatorImpl::OnOSMemoryDumpForVMRegions,
                          weak_ptr_factory_.GetWeakPtr(), dump_guid);
  QueuedRequestDispatcher::SetUpAndDispatchVmRegionRequest(request_ptr, clients,
                                                           pids, os_callback);
  FinalizeVmRegionDumpIfAllManagersReplied(dump_guid);
}

void CoordinatorImpl::UnregisterClientProcess(base::ProcessId process_id) {
  QueuedRequest* request = GetCurrentRequest();
  if (request != nullptr) {
    // Check if we are waiting for an ack from this client process.
    auto it = request->pending_responses.begin();
    while (it != request->pending_responses.end()) {
      // The calls to On*MemoryDumpResponse below, if executed, will delete the
      // element under the iterator which invalidates it. To avoid this we
      // increment the iterator in advance while keeping a reference to the
      // current element.
      std::set<QueuedRequest::PendingResponse>::iterator current = it++;
      if (current->process_id != process_id)
        continue;
      RemovePendingResponse(process_id, current->type);
      DLOG(ERROR)
          << "Memory dump request failed due to disconnected child process "
          << process_id;
      request->failed_memory_dump_count++;
    }
    FinalizeGlobalMemoryDumpIfAllManagersReplied();
  }

  for (auto& pair : in_progress_vm_region_requests_) {
    QueuedVmRegionRequest* in_progress_request = pair.second.get();
    auto it = in_progress_request->pending_responses.begin();
    while (it != in_progress_request->pending_responses.end()) {
      auto current = it++;
      if (*current == process_id) {
        in_progress_request->pending_responses.erase(current);
      }
    }
  }

  // Try to finalize all outstanding vm region requests.
  for (auto& pair : in_progress_vm_region_requests_) {
    // PostTask to avoid re-entrancy or modification of data-structure during
    // iteration.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CoordinatorImpl::FinalizeVmRegionDumpIfAllManagersReplied,
            weak_ptr_factory_.GetWeakPtr(), pair.second->dump_guid));
  }

  size_t num_deleted = clients_.erase(process_id);
  DCHECK(num_deleted == 1);
}

void CoordinatorImpl::RequestGlobalMemoryDumpInternal(
    const QueuedRequest::Args& args,
    RequestGlobalMemoryDumpInternalCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  bool another_dump_is_queued = !queued_memory_dump_requests_.empty();

  // If this is a periodic or peak memory dump request and there already is
  // another request in the queue with the same level of detail, there's no
  // point in enqueuing this request.
  if (another_dump_is_queued &&
      args.dump_type == MemoryDumpType::kPeriodicInterval) {
    for (const auto& request : queued_memory_dump_requests_) {
      if (request.args.level_of_detail == args.level_of_detail) {
        VLOG(1) << "RequestGlobalMemoryDump("
                << base::trace_event::MemoryDumpTypeToString(args.dump_type)
                << ") skipped because another dump request with the same "
                   "level of detail ("
                << base::trace_event::MemoryDumpLevelOfDetailToString(
                       args.level_of_detail)
                << ") is already in the queue";
        std::move(callback).Run(false /* success */, 0 /* dump_guid */,
                                nullptr /* global_memory_dump */);
        return;
      }
    }
  }

  queued_memory_dump_requests_.emplace_back(args, ++next_dump_id_,
                                            std::move(callback));

  // If another dump is already in queued, this dump will automatically be
  // scheduled when the other dump finishes.
  if (another_dump_is_queued)
    return;

  PerformNextQueuedGlobalMemoryDump();
}

void CoordinatorImpl::OnQueuedRequestTimedOut(uint64_t dump_guid) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  QueuedRequest* request = GetCurrentRequest();

  // TODO(lalitm): add metrics for how often this happens.

  // Only consider the current request timed out if we fired off this
  // delayed callback in association with this request.
  if (!request || request->dump_guid != dump_guid)
    return;

  // Fail all remaining dumps being waited upon and clear the vector.
  if (request->pending_responses.size() > 0) {
    DLOG(ERROR) << "Global dump request timed out waiting for "
                << request->pending_responses.size() << " requests";
  }
  request->failed_memory_dump_count += request->pending_responses.size();
  request->pending_responses.clear();

  // Callback the consumer of the service.
  FinalizeGlobalMemoryDumpIfAllManagersReplied();
}

void CoordinatorImpl::OnHeapDumpTimeOut(uint64_t dump_guid) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  QueuedRequest* request = GetCurrentRequest();

  // TODO(lalitm): add metrics for how often this happens.

  // Only consider the current request timed out if we fired off this
  // delayed callback in association with this request.
  if (!request || request->dump_guid != dump_guid)
    return;

  // Fail all remaining dumps being waited upon and clear the vector.
  if (request->heap_dump_in_progress) {
    request->heap_dump_in_progress = false;
    FinalizeGlobalMemoryDumpIfAllManagersReplied();
  }
}

void CoordinatorImpl::PerformNextQueuedGlobalMemoryDump() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  QueuedRequest* request = GetCurrentRequest();

  if (request == nullptr)
    return;

  std::vector<QueuedRequestDispatcher::ClientInfo> clients;
  for (const auto& entry : clients_) {
    const base::ProcessId pid = entry.first;
    clients.emplace_back(entry.second->client.get(), pid,
                         entry.second->process_type,
                         entry.second->service_name);
  }

  auto chrome_callback =
      base::BindRepeating(&CoordinatorImpl::OnChromeMemoryDumpResponse,
                          weak_ptr_factory_.GetWeakPtr());
  auto os_callback =
      base::BindRepeating(&CoordinatorImpl::OnOSMemoryDumpResponse,
                          weak_ptr_factory_.GetWeakPtr(), request->dump_guid);
  QueuedRequestDispatcher::SetUpAndDispatch(request, clients, chrome_callback,
                                            os_callback);

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CoordinatorImpl::OnQueuedRequestTimedOut,
                     weak_ptr_factory_.GetWeakPtr(), request->dump_guid),
      client_process_timeout_);

  if (request->args.add_to_trace && heap_profiler_) {
    request->heap_dump_in_progress = true;

    // |IsArgumentFilterEnabled| is the round-about way of asking to anonymize
    // the trace. The only way that PII gets leaked is if the full path is
    // emitted for mapped files. Passing |strip_path_from_mapped_files|
    // is all that is necessary to anonymize the trace.
    bool strip_path_from_mapped_files =
        base::trace_event::TraceLog::GetInstance()
            ->GetCurrentTraceConfig()
            .IsArgumentFilterEnabled();
    heap_profiler_->DumpProcessesForTracing(
        strip_path_from_mapped_files, write_proto_heap_profile_,
        base::BindOnce(&CoordinatorImpl::OnDumpProcessesForTracing,
                       weak_ptr_factory_.GetWeakPtr(), request->dump_guid));

    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CoordinatorImpl::OnHeapDumpTimeOut,
                       weak_ptr_factory_.GetWeakPtr(), request->dump_guid),
        kHeapDumpTimeout);
  }

  // Run the callback in case there are no client processes registered.
  FinalizeGlobalMemoryDumpIfAllManagersReplied();
}

QueuedRequest* CoordinatorImpl::GetCurrentRequest() {
  if (queued_memory_dump_requests_.empty()) {
    return nullptr;
  }
  return &queued_memory_dump_requests_.front();
}

void CoordinatorImpl::OnChromeMemoryDumpResponse(
    base::ProcessId process_id,
    bool success,
    uint64_t dump_guid,
    std::unique_ptr<base::trace_event::ProcessMemoryDump> chrome_memory_dump) {
  using ResponseType = QueuedRequest::PendingResponse::Type;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  QueuedRequest* request = GetCurrentRequest();
  if (request == nullptr || request->dump_guid != dump_guid) {
    return;
  }

  RemovePendingResponse(process_id, ResponseType::kChromeDump);

  if (!base::Contains(clients_, process_id)) {
    VLOG(1) << "Received a memory dump response from an unregistered client";
    return;
  }

  auto* response = &request->responses[process_id];
  response->chrome_dump = std::move(chrome_memory_dump);

  if (!success) {
    DLOG(ERROR) << "Memory dump request failed: NACK from client process";
    request->failed_memory_dump_count++;
  }

  FinalizeGlobalMemoryDumpIfAllManagersReplied();
}

void CoordinatorImpl::OnOSMemoryDumpResponse(uint64_t dump_guid,
                                             base::ProcessId process_id,
                                             bool success,
                                             OSMemDumpMap os_dumps) {
  using ResponseType = QueuedRequest::PendingResponse::Type;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  QueuedRequest* request = GetCurrentRequest();
  if (request == nullptr || request->dump_guid != dump_guid) {
    return;
  }

  RemovePendingResponse(process_id, ResponseType::kOSDump);

  if (!base::Contains(clients_, process_id)) {
    VLOG(1) << "Received a memory dump response from an unregistered client";
    return;
  }

  request->responses[process_id].os_dumps = std::move(os_dumps);

  if (!success) {
    DLOG(ERROR) << "Memory dump request failed: NACK from client process";
    request->failed_memory_dump_count++;
  }

  FinalizeGlobalMemoryDumpIfAllManagersReplied();
}

void CoordinatorImpl::OnOSMemoryDumpForVMRegions(uint64_t dump_guid,
                                                 base::ProcessId process_id,
                                                 bool success,
                                                 OSMemDumpMap os_dumps) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto request_it = in_progress_vm_region_requests_.find(dump_guid);
  CHECK(request_it != in_progress_vm_region_requests_.end(),
        base::NotFatalUntil::M130);

  QueuedVmRegionRequest* request = request_it->second.get();
  auto it = request->pending_responses.find(process_id);
  CHECK(it != request->pending_responses.end(), base::NotFatalUntil::M130);
  request->pending_responses.erase(it);
  request->responses[process_id].os_dumps = std::move(os_dumps);

  FinalizeVmRegionDumpIfAllManagersReplied(request->dump_guid);
}

void CoordinatorImpl::FinalizeVmRegionDumpIfAllManagersReplied(
    uint64_t dump_guid) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = in_progress_vm_region_requests_.find(dump_guid);
  if (it == in_progress_vm_region_requests_.end())
    return;

  if (!it->second->pending_responses.empty())
    return;

  QueuedRequestDispatcher::VmRegions results =
      QueuedRequestDispatcher::FinalizeVmRegionRequest(it->second.get());
  std::move(it->second->callback).Run(std::move(results));
  in_progress_vm_region_requests_.erase(it);
}

void CoordinatorImpl::OnDumpProcessesForTracing(
    uint64_t dump_guid,
    std::vector<mojom::HeapProfileResultPtr> heap_profile_results) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  QueuedRequest* request = GetCurrentRequest();
  if (!request || request->dump_guid != dump_guid) {
    return;
  }

  request->heap_dump_in_progress = false;

  for (auto& result : heap_profile_results) {
    base::trace_event::TraceArguments args(
        "dumps", std::make_unique<StringWrapper>(std::move(result->json)));

    // Using the same id merges all of the heap dumps into a single detailed
    // dump node in the UI.
    TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_PROCESS_ID(
        TRACE_EVENT_PHASE_MEMORY_DUMP,
        base::trace_event::TraceLog::GetCategoryGroupEnabled(
            base::trace_event::MemoryDumpManager::kTraceCategory),
        "periodic_interval", trace_event_internal::kGlobalScope, dump_guid,
        result->pid, &args, TRACE_EVENT_FLAG_HAS_ID);
  }

  FinalizeGlobalMemoryDumpIfAllManagersReplied();
}

void CoordinatorImpl::RemovePendingResponse(
    base::ProcessId process_id,
    QueuedRequest::PendingResponse::Type type) {
  QueuedRequest* request = GetCurrentRequest();
  if (request == nullptr) {
    NOTREACHED_IN_MIGRATION() << "No current dump request.";
    return;
  }
  auto it = request->pending_responses.find({process_id, type});
  if (it == request->pending_responses.end()) {
    VLOG(1) << "Unexpected memory dump response";
    return;
  }
  request->pending_responses.erase(it);
}

void CoordinatorImpl::FinalizeGlobalMemoryDumpIfAllManagersReplied() {
  TRACE_EVENT0(base::trace_event::MemoryDumpManager::kTraceCategory,
               "GlobalMemoryDump.Computation");
  DCHECK(!queued_memory_dump_requests_.empty());

  QueuedRequest* request = &queued_memory_dump_requests_.front();
  if (!request->dump_in_progress || request->pending_responses.size() > 0 ||
      request->heap_dump_in_progress) {
    return;
  }

  QueuedRequestDispatcher::Finalize(request,
                                    TracingObserverProto::GetInstance());

  queued_memory_dump_requests_.pop_front();
  request = nullptr;

  // Schedule the next queued dump (if applicable).
  if (!queued_memory_dump_requests_.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&CoordinatorImpl::PerformNextQueuedGlobalMemoryDump,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

CoordinatorImpl::ClientInfo::ClientInfo(
    mojo::Remote<mojom::ClientProcess> client,
    mojom::ProcessType process_type,
    std::optional<std::string> service_name)
    : client(std::move(client)),
      process_type(process_type),
      service_name(std::move(service_name)) {}

CoordinatorImpl::ClientInfo::~ClientInfo() = default;

}  // namespace memory_instrumentation
