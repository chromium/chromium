// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_memory_inspector.h"

#include <lib/fpromise/promise.h>
#include <lib/inspect/cpp/inspector.h>
#include <sstream>

#include "base/trace_event/memory_dump_request_args.h"
#include "fuchsia/base/config_reader.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

namespace {

std::vector<std::string> GetAllocatorDumpNamesFromConfig() {
  const absl::optional<base::Value>& config = cr_fuchsia::LoadPackageConfig();
  if (!config)
    return {};

  const base::Value* names_list = config->FindListPath("allocator-dump-names");
  if (!names_list)
    return {};

  std::vector<std::string> names;
  names.reserve(names_list->GetList().size());
  for (auto& name : names_list->GetList()) {
    names.push_back(name.GetString());
  }
  return names;
}

// List of allocator dump names to include.
const std::vector<std::string>& AllocatorDumpNames() {
  static base::NoDestructor<std::vector<std::string>> allocator_dump_names(
      GetAllocatorDumpNamesFromConfig());
  return *allocator_dump_names;
}

}  // namespace

WebEngineMemoryInspector::WebEngineMemoryInspector(inspect::Node& parent_node) {
  // Loading the allocator dump names from the config involves blocking I/O so
  // trigger it to be done during construction, before the prohibition on
  // blocking the main thread is applied.
  AllocatorDumpNames();

  node_ = parent_node.CreateLazyNode("memory", [this]() {
    return fpromise::make_promise(fit::bind_member(
        this, &WebEngineMemoryInspector::ResolveMemoryDumpPromise));
  });
}

WebEngineMemoryInspector::~WebEngineMemoryInspector() = default;

fpromise::result<inspect::Inspector>
WebEngineMemoryInspector::ResolveMemoryDumpPromise(fpromise::context& context) {
  // If there is a |dump_results_| then resolve the promise with it.
  if (dump_results_) {
    auto memory_dump = std::move(dump_results_);
    return fpromise::ok(*memory_dump);
  }

  // If MemoryInstrumentation is not initialized then resolve an error.
  auto* memory_instrumentation =
      memory_instrumentation::MemoryInstrumentation::GetInstance();
  if (!memory_instrumentation)
    return fpromise::error();

  // Request memory usage summaries for all processes, including details for
  // any configured allocator dumps.
  auto* coordinator = memory_instrumentation->GetCoordinator();
  DCHECK(coordinator);

  coordinator->RequestGlobalMemoryDump(
      base::trace_event::MemoryDumpType::SUMMARY_ONLY,
      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND,
      base::trace_event::MemoryDumpDeterminism::NONE, AllocatorDumpNames(),
      base::BindOnce(&WebEngineMemoryInspector::OnMemoryDumpComplete,
                     weak_this_.GetWeakPtr(), context.suspend_task()));

  return fpromise::pending();
}

void WebEngineMemoryInspector::OnMemoryDumpComplete(
    fpromise::suspended_task task,
    bool success,
    memory_instrumentation::mojom::GlobalMemoryDumpPtr raw_dump) {
  DCHECK(!dump_results_);

  dump_results_ = std::make_unique<inspect::Inspector>();

  // If capture failed then there is no data to report.
  if (!success || !raw_dump) {
    task.resume_task();
    return;
  }

  for (const auto& process_dump : raw_dump->process_dumps) {
    auto node = dump_results_->GetRoot().CreateChild(
        base::NumberToString(process_dump->pid));

    // Include details of each process' role in the web instance.
    std::ostringstream type;
    type << process_dump->process_type;
    node.CreateString("type", type.str(), dump_results_.get());

    const auto service_name = process_dump->service_name;
    if (service_name) {
      node.CreateString("service", *service_name, dump_results_.get());
    }

    // Include the summary of the process' memory usage.
    const auto& os_dump = process_dump->os_dump;
    node.CreateUint("resident_kb", os_dump->resident_set_kb,
                    dump_results_.get());
    node.CreateUint("private_kb", os_dump->private_footprint_kb,
                    dump_results_.get());
    node.CreateUint("shared_kb", os_dump->shared_footprint_kb,
                    dump_results_.get());

    // If provided, include detail from individual allocators.
    auto detail_node = node.CreateChild("allocator_dump");
    if (!process_dump->chrome_allocator_dumps.empty()) {
      for (auto& it : process_dump->chrome_allocator_dumps) {
        // Create a node using the allocator dump name.
        auto allocator_node = detail_node.CreateChild(it.first);

        // Publish the allocator-provided fields into the node.
        for (auto& field : it.second->numeric_entries) {
          allocator_node.CreateUint(field.first, field.second,
                                    dump_results_.get());
        }

        dump_results_->emplace(std::move(allocator_node));
      }

      dump_results_->emplace(std::move(detail_node));
    }

    dump_results_->emplace(std::move(node));
  }

  task.resume_task();
}
