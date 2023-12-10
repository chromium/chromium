// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/web_engine_memory_inspector.h"

#include <lib/fpromise/promise.h>
#include <lib/inspect/cpp/inspector.h>
#include <sstream>

#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "components/fuchsia_component_support/config_reader.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

namespace {

std::vector<std::string> GetAllocatorDumpNamesFromConfig() {
  const std::optional<base::Value::Dict>& config =
      fuchsia_component_support::LoadPackageConfig();
  if (!config)
    return {};

  const base::Value::List* names_list =
      config->FindList("allocator-dump-names");
  if (!names_list)
    return {};

  std::vector<std::string> names;
  names.reserve(names_list->size());
  for (auto& name : *names_list) {
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

// Returns true if every field in the supplied |dump|, and those of its
// children, are zero.  Generally parent nodes summarize the total usage across
// all of their children, such that if the parent is all-zero then the children
// must also be all-zero. This implementation is optimized for the case in
// which that property holds, but also copes gracefully when it does not.
bool AreAllDumpEntriesZero(
    const memory_instrumentation::mojom::AllocatorMemDump* dump) {
  for (auto& it : dump->numeric_entries) {
    if (it.second != 0u)
      return false;
  }
  for (auto& it : dump->children) {
    if (!AreAllDumpEntriesZero(it.second.get()))
      return false;
  }
  return true;
}

// Creates a node |name|, under |parent|, populated recursively with the
// contents of |dump|. The returned tree of Nodes are emplace()d to be owned by
// the specified |owner|.
inspect::Node NodeFromAllocatorMemDump(
    inspect::Inspector* owner,
    inspect::Node* parent,
    const std::string& name,
    const memory_instrumentation::mojom::AllocatorMemDump* dump) {
  auto node = parent->CreateChild(name);

  // Add subordinate nodes for any children.
  std::vector<const memory_instrumentation::mojom::AllocatorMemDump*> children;
  children.reserve(dump->children.size());
  for (auto& it : dump->children) {
    // If a child contains no information then omit it.
    if (AreAllDumpEntriesZero(it.second.get()))
      continue;

    children.push_back(it.second.get());
    owner->emplace(
        NodeFromAllocatorMemDump(owner, &node, it.first, it.second.get()));
  }

  // Publish the allocator-provided fields into the node.  Entries are not
  // published if there is a single child, with identical entries, to avoid
  // redundancy in the emitted output.
  bool same_as_child =
      (children.size() == 1u &&
       (*children.begin())->numeric_entries == dump->numeric_entries);
  if (!same_as_child) {
    for (auto& it : dump->numeric_entries) {
      node.CreateUint(it.first, it.second, owner);
    }
  }

  return node;
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
      base::trace_event::MemoryDumpType::kSummaryOnly,
      base::trace_event::MemoryDumpLevelOfDetail::kBackground,
      base::trace_event::MemoryDumpDeterminism::kNone, AllocatorDumpNames(),
      base::BindOnce(&WebEngineMemoryInspector::OnMemoryDumpComplete,
                     weak_this_.GetMutableWeakPtr(), base::TimeTicks::Now(),
                     context.suspend_task()));

  return fpromise::pending();
}

void WebEngineMemoryInspector::OnMemoryDumpComplete(
    base::TimeTicks requested_at,
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

  // Note the delay between requesting the dump, and it being started.
  dump_results_->GetRoot().CreateDouble(
      "dump_queued_duration_ms",
      (raw_dump->start_time - requested_at).InMillisecondsF(),
      dump_results_.get());

  // Note the delay between starting the dump, and it completing.
  dump_results_->GetRoot().CreateDouble(
      "dump_duration_ms",
      (base::TimeTicks::Now() - raw_dump->start_time).InMillisecondsF(),
      dump_results_.get());

  for (const auto& process_dump : raw_dump->process_dumps) {
    auto node = dump_results_->GetRoot().CreateChild(
        base::NumberToString(process_dump->pid));

    // Include details of each process' role in the web instance.
    std::ostringstream type;
    type << process_dump->process_type;
    static const inspect::StringReference kTypeNodeName("type");
    node.CreateString(kTypeNodeName, type.str(), dump_results_.get());

    const auto service_name = process_dump->service_name;
    if (service_name) {
      static const inspect::StringReference kServiceNodeName("service");
      node.CreateString(kServiceNodeName, *service_name, dump_results_.get());
    }

    // Include the summary of the process' memory usage.
    const auto& os_dump = process_dump->os_dump;
    static const inspect::StringReference kResidentKbNodeName("resident_kb");
    node.CreateUint(kResidentKbNodeName, os_dump->resident_set_kb,
                    dump_results_.get());
    static const inspect::StringReference kPrivateKbNodeName("private_kb");
    node.CreateUint(kPrivateKbNodeName, os_dump->private_footprint_kb,
                    dump_results_.get());
    static const inspect::StringReference kSharedKbNodeName("shared_kb");
    node.CreateUint(kSharedKbNodeName, os_dump->shared_footprint_kb,
                    dump_results_.get());

    // If provided, include detail from individual allocators.
    if (!process_dump->chrome_allocator_dumps.empty()) {
      static const inspect::StringReference kAllocatorDumpNodeName(
          "allocator_dump");
      auto detail_node = node.CreateChild(kAllocatorDumpNodeName);

      for (auto& it : process_dump->chrome_allocator_dumps) {
        dump_results_->emplace(NodeFromAllocatorMemDump(
            dump_results_.get(), &detail_node, it.first, it.second.get()));
      }

      dump_results_->emplace(std::move(detail_node));
    }

    dump_results_->emplace(std::move(node));
  }

  task.resume_task();
}
