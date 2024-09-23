// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/queued_request_dispatcher.h"

#include <inttypes.h>

#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "services/resource_coordinator/memory_instrumentation/aggregate_metrics_processor.h"
#include "services/resource_coordinator/memory_instrumentation/memory_dump_map_converter.h"
#include "services/resource_coordinator/memory_instrumentation/switches.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer_proto.h"
#include "third_party/perfetto/include/perfetto/ext/trace_processor/importers/memory_tracker/graph_processor.h"
#include "third_party/perfetto/protos/perfetto/trace/memory_graph.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

using base::trace_event::TracedValue;
using perfetto::trace_processor::GlobalNodeGraph;
using perfetto::trace_processor::LevelOfDetail;
using perfetto::trace_processor::RawMemoryGraphNode;
using Node = perfetto::trace_processor::GlobalNodeGraph::Node;
using perfetto::trace_processor::GraphProcessor;

namespace memory_instrumentation {

namespace {

// Returns the private memory footprint calculated from given |os_dump|.
//
// See design docs linked in the bugs for the rationale of the computation:
// - Linux/Android: https://crbug.com/707019 .
// - Mac OS: https://crbug.com/707021 .
// - Win: https://crbug.com/707022 .
uint32_t CalculatePrivateFootprintKb(const mojom::RawOSMemDump& os_dump,
                                     uint32_t shared_resident_kb) {
  DCHECK(os_dump.platform_private_footprint);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_FUCHSIA)
  uint64_t rss_anon_bytes = os_dump.platform_private_footprint->rss_anon_bytes;
  uint64_t vm_swap_bytes = os_dump.platform_private_footprint->vm_swap_bytes;
  return (rss_anon_bytes + vm_swap_bytes) / 1024;
#elif BUILDFLAG(IS_MAC)
  uint64_t phys_footprint_bytes =
      os_dump.platform_private_footprint->phys_footprint_bytes;
  return base::saturated_cast<uint32_t>(
      base::saturated_cast<int32_t>(phys_footprint_bytes / 1024) -
      base::saturated_cast<int32_t>(shared_resident_kb));
#elif BUILDFLAG(IS_WIN)
  return base::saturated_cast<int32_t>(
      os_dump.platform_private_footprint->private_bytes / 1024);
#else
  // TODO(crbug.com/40947218): Implement for iOS.
  return 0;
#endif
}

memory_instrumentation::mojom::OSMemDumpPtr CreatePublicOSDump(
    const mojom::RawOSMemDump& internal_os_dump,
    uint32_t shared_resident_kb) {
  mojom::OSMemDumpPtr os_dump = mojom::OSMemDump::New();

  os_dump->resident_set_kb = internal_os_dump.resident_set_kb;
  os_dump->peak_resident_set_kb = internal_os_dump.peak_resident_set_kb;
  os_dump->is_peak_rss_resettable = internal_os_dump.is_peak_rss_resettable;
  os_dump->private_footprint_kb =
      CalculatePrivateFootprintKb(internal_os_dump, shared_resident_kb);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  os_dump->private_footprint_swap_kb =
      internal_os_dump.platform_private_footprint->vm_swap_bytes / 1024;
#endif
  return os_dump;
}

void NodeAsValueIntoRecursively(const GlobalNodeGraph::Node& node,
                                TracedValue* value,
                                std::vector<std::string_view>* path) {
  // Don't dump the root node.
  if (!path->empty()) {
    std::string name = base::JoinString(*path, "/");
    value->BeginDictionaryWithCopiedName(name);

    if (!node.id().empty())
      value->SetString("id", node.id().ToString());

    value->BeginDictionary("attrs");
    for (const auto& name_to_entry : node.const_entries()) {
      const auto& entry = name_to_entry.second;
      value->BeginDictionaryWithCopiedName(name_to_entry.first);
      switch (entry.type) {
        case GlobalNodeGraph::Node::Entry::kUInt64:
          value->SetString("type", RawMemoryGraphNode::kTypeScalar);
          value->SetString("value",
                           base::StringPrintf("%" PRIx64, entry.value_uint64));
          break;
        case GlobalNodeGraph::Node::Entry::kString:
          value->SetString("type", RawMemoryGraphNode::kTypeString);
          value->SetString("value", entry.value_string);
          break;
      }
      switch (entry.units) {
        case GlobalNodeGraph::Node::Entry::ScalarUnits::kBytes:
          value->SetString("units", RawMemoryGraphNode::kUnitsBytes);
          break;
        case GlobalNodeGraph::Node::Entry::ScalarUnits::kObjects:
          value->SetString("units", RawMemoryGraphNode::kUnitsObjects);
          break;
      }
      value->EndDictionary();
    }
    value->EndDictionary();  // "attrs": { ... }

    if (node.is_weak())
      value->SetInteger("flags", RawMemoryGraphNode::Flags::kWeak);

    value->EndDictionary();  // "allocator_name/heap_subheap": { ... }
  }

  for (const auto& name_to_child : node.const_children()) {
    path->push_back(name_to_child.first);
    NodeAsValueIntoRecursively(*name_to_child.second, value, path);
    path->pop_back();
  }
}

mojom::AllocatorMemDumpPtr CreateAllocatorDumpForNode(const Node* node,
                                                      bool recursive) {
  base::flat_map<std::string, uint64_t> numeric_entries;
  for (const auto& entry : node->const_entries()) {
    if (entry.second.type == Node::Entry::Type::kUInt64)
      numeric_entries.emplace(entry.first, entry.second.value_uint64);
  }
  base::flat_map<std::string, mojom::AllocatorMemDumpPtr> children;
  if (recursive) {
    for (const auto& child : node->const_children()) {
      children.emplace(child.first,
                       CreateAllocatorDumpForNode(child.second, true));
    }
  }
  return mojom::AllocatorMemDump::New(std::move(numeric_entries),
                                      std::move(children));
}

}  // namespace

// static
void QueuedRequestDispatcher::SetUpAndDispatch(
    QueuedRequest* request,
    const std::vector<ClientInfo>& clients,
    const ChromeCallback& chrome_callback,
    const OsCallback& os_callback) {
  using ResponseType = QueuedRequest::PendingResponse::Type;
  DCHECK(!request->dump_in_progress);
  request->dump_in_progress = true;

  request->start_time = base::TimeTicks::Now();

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(
      base::trace_event::MemoryDumpManager::kTraceCategory, "GlobalMemoryDump",
      TRACE_ID_LOCAL(request->dump_guid), "dump_type",
      base::trace_event::MemoryDumpTypeToString(request->args.dump_type),
      "level_of_detail",
      base::trace_event::MemoryDumpLevelOfDetailToString(
          request->args.level_of_detail));

  request->failed_memory_dump_count = 0;

  // Note: the service process itself is registered as a ClientProcess and
  // will be treated like any other process for the sake of memory dumps.
  request->pending_responses.clear();

  for (const auto& client_info : clients) {
    mojom::ClientProcess* client = client_info.client;

    // If we're only looking for a single pid process, then ignore clients
    // with different pid.
    if (request->args.pid != base::kNullProcessId &&
        request->args.pid != client_info.pid) {
      continue;
    }

    request->responses[client_info.pid].process_id = client_info.pid;
    request->responses[client_info.pid].process_type = client_info.process_type;
    request->responses[client_info.pid].service_name = client_info.service_name;

    // Don't request a chrome memory dump at all if the client only wants the
    // a memory footprint.
    //
    // This must occur before the call to RequestOSMemoryDump, as
    // ClientProcessImpl will [for macOS], delay the calculations for the
    // OSMemoryDump until the Chrome memory dump is finished. See
    // https://bugs.chromium.org/p/chromium/issues/detail?id=812346#c16 for more
    // details.
    if (!request->args.memory_footprint_only) {
      request->pending_responses.insert(
          {client_info.pid, ResponseType::kChromeDump});
      client->RequestChromeMemoryDump(
          request->GetRequestArgs(),
          base::BindOnce(chrome_callback, client_info.pid));
    }

// On most platforms each process can dump data about their own process
// so ask each process to do so Linux is special see below.
#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
    request->pending_responses.insert({client_info.pid, ResponseType::kOSDump});
    client->RequestOSMemoryDump(request->memory_map_option(),
                                {base::kNullProcessId},
                                base::BindOnce(os_callback, client_info.pid));
#endif  // !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)

    // If we are in the single pid case, then we've already found the only
    // process we're looking for.
    if (request->args.pid != base::kNullProcessId)
      break;
  }

// In some cases, OS stats can only be dumped from a privileged process to
// get around to sandboxing/selinux restrictions (see crbug.com/461788).
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  std::vector<base::ProcessId> pids;
  mojom::ClientProcess* browser_client = nullptr;
  base::ProcessId browser_client_pid = base::kNullProcessId;
  pids.reserve(request->args.pid == base::kNullProcessId ? clients.size() : 1);
  for (const auto& client_info : clients) {
    if (request->args.pid == base::kNullProcessId ||
        client_info.pid == request->args.pid) {
      pids.push_back(client_info.pid);
    }
    if (client_info.process_type == mojom::ProcessType::BROWSER) {
      browser_client = client_info.client;
      browser_client_pid = client_info.pid;
    }
  }
  if (clients.size() > 0) {
    DCHECK(browser_client);
  }
  if (browser_client && pids.size() > 0) {
    request->pending_responses.insert(
        {browser_client_pid, ResponseType::kOSDump});
    auto callback = base::BindOnce(os_callback, browser_client_pid);
    browser_client->RequestOSMemoryDump(request->memory_map_option(), pids,
                                        std::move(callback));
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  // In this case, we have not found the pid we are looking for so increment
  // the failed dump count and exit.
  if (request->args.pid != base::kNullProcessId &&
      request->pending_responses.empty()) {
    DLOG(ERROR) << "Memory dump request failed due to missing pid "
                << request->args.pid;
    request->failed_memory_dump_count++;
    return;
  }
}

// static
void QueuedRequestDispatcher::SetUpAndDispatchVmRegionRequest(
    QueuedVmRegionRequest* request,
    const std::vector<ClientInfo>& clients,
    const std::vector<base::ProcessId>& desired_pids,
    const OsCallback& os_callback) {
// On Linux, OS stats can only be dumped from a privileged process to
// get around to sandboxing/selinux restrictions (see crbug.com/461788).
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  mojom::ClientProcess* browser_client = nullptr;
  base::ProcessId browser_client_pid = 0;
  for (const auto& client_info : clients) {
    if (client_info.process_type == mojom::ProcessType::BROWSER) {
      browser_client = client_info.client;
      browser_client_pid = client_info.pid;
      break;
    }
  }

  if (!browser_client) {
    DLOG(ERROR) << "Missing browser client.";
    return;
  }

  request->pending_responses.insert(browser_client_pid);
  request->responses[browser_client_pid].process_id = browser_client_pid;
  auto callback = base::BindOnce(os_callback, browser_client_pid);
  browser_client->RequestOSMemoryDump(mojom::MemoryMapOption::MODULES,
                                      desired_pids, std::move(callback));
#else
  for (const auto& client_info : clients) {
    if (base::Contains(desired_pids, client_info.pid)) {
      mojom::ClientProcess* client = client_info.client;
      request->pending_responses.insert(client_info.pid);
      request->responses[client_info.pid].process_id = client_info.pid;
      request->responses[client_info.pid].service_name =
          client_info.service_name;
      client->RequestOSMemoryDump(mojom::MemoryMapOption::MODULES,
                                  {base::kNullProcessId},
                                  base::BindOnce(os_callback, client_info.pid));
    }
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
}

// static
QueuedRequestDispatcher::VmRegions
QueuedRequestDispatcher::FinalizeVmRegionRequest(
    QueuedVmRegionRequest* request) {
  VmRegions results;
  for (auto& response : request->responses) {
    const base::ProcessId& original_pid = response.second.process_id;

    // |response| accumulates the replies received by each client process.
    // On Linux, the browser process will provide all OS dumps. On non-Linux,
    // each client process provides 1 OS dump, % the case where the client is
    // disconnected mid dump.
    OSMemDumpMap& extra_os_dumps = response.second.os_dumps;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    for (auto& kv : extra_os_dumps) {
      auto pid = kv.first == base::kNullProcessId ? original_pid : kv.first;
      DCHECK(results.find(pid) == results.end());
      results.emplace(pid, std::move(kv.second->memory_maps));
    }
#else
    // This can be empty if the client disconnects before providing both
    // dumps. See UnregisterClientProcess().
    DCHECK_LE(extra_os_dumps.size(), 1u);
    for (auto& kv : extra_os_dumps) {
      // When the OS dump comes from child processes, the pid is supposed to be
      // not used. We know the child process pid at the time of the request and
      // also wouldn't trust pids coming from child processes.
      DCHECK_EQ(base::kNullProcessId, kv.first);

      // Check we don't receive duplicate OS dumps for the same process.
      DCHECK(results.find(original_pid) == results.end());

      results.emplace(original_pid, std::move(kv.second->memory_maps));
    }
#endif
  }
  return results;
}

void QueuedRequestDispatcher::Finalize(QueuedRequest* request,
                                       TracingObserver* tracing_observer) {
  DCHECK(request->dump_in_progress);
  DCHECK(request->pending_responses.empty());

  // Reconstruct a map of pid -> ProcessMemoryDump by reassembling the responses
  // received by the clients for this dump. In some cases the response coming
  // from one client can also provide the dump of OS counters for other
  // processes. A concrete case is Linux, where the browser process provides
  // details for the child processes to get around sandbox restrictions on
  // opening /proc pseudo files.

  // All the pointers in the maps will continue to be owned by |request|
  // which outlives these containers.
  std::map<base::ProcessId, mojom::ProcessType> pid_to_process_type;
  std::map<base::ProcessId, const base::trace_event::ProcessMemoryDump*>
      pid_to_pmd;
  std::map<base::ProcessId, mojom::RawOSMemDump*> pid_to_os_dump;
  for (auto& response : request->responses) {
    const base::ProcessId& original_pid = response.second.process_id;
    pid_to_process_type[original_pid] = response.second.process_type;

    // |chrome_dump| can be nullptr if this was a OS-counters only response.
    pid_to_pmd[original_pid] = response.second.chrome_dump.get();

    // |response| accumulates the replies received by each client process.
    // Depending on the OS each client process might return 1 chrome + 1 OS
    // dump each or, in the case of Linux, only 1 chrome dump each % the
    // browser process which will provide all the OS dumps.
    // In the former case (!OS_LINUX) we expect client processes to have
    // exactly one OS dump in their |response|, % the case when they
    // unexpectedly disconnect in the middle of a dump (e.g. because they
    // crash). In the latter case (OS_LINUX) we expect the full map to come
    // from the browser process response.
    OSMemDumpMap& extra_os_dumps = response.second.os_dumps;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    for (const auto& kv : extra_os_dumps) {
      auto pid = kv.first == base::kNullProcessId ? original_pid : kv.first;
      DCHECK_EQ(pid_to_os_dump[pid], nullptr);
      pid_to_os_dump[pid] = kv.second.get();
    }
#else
    // This can be empty if the client disconnects before providing both
    // dumps. See UnregisterClientProcess().
    DCHECK_LE(extra_os_dumps.size(), 1u);
    for (const auto& kv : extra_os_dumps) {
      // When the OS dump comes from child processes, the pid is supposed to be
      // not used. We know the child process pid at the time of the request and
      // also wouldn't trust pids coming from child processes.
      DCHECK_EQ(base::kNullProcessId, kv.first);

      // Check we don't receive duplicate OS dumps for the same process.
      DCHECK_EQ(pid_to_os_dump[original_pid], nullptr);

      pid_to_os_dump[original_pid] = kv.second.get();
    }
#endif
  }

  MemoryDumpMapConverter converter;
  perfetto::trace_processor::GraphProcessor::RawMemoryNodeMap perfettoNodeMap =
      converter.Convert(pid_to_pmd);

  // Generate the global memory graph from the map of pids to dumps, removing
  // weak nodes.
  // TODO (crbug.com/1112671): We should avoid graph processing once we moved
  // the shared footprint computation to perfetto.
  std::unique_ptr<GlobalNodeGraph> global_graph =
      GraphProcessor::CreateMemoryGraph(perfettoNodeMap);
  GraphProcessor::RemoveWeakNodesFromGraph(global_graph.get());

  // Compute the shared memory footprint for each process from the graph.
  auto original =
      GraphProcessor::ComputeSharedFootprintFromGraph(*global_graph);
  std::map<base::ProcessId, uint64_t> shared_footprints;
  for (const auto& item : original) {
    shared_footprints.emplace(static_cast<base::ProcessId>(item.first),
                              item.second);
  }
  // Perform the rest of the computation on the graph.
  GraphProcessor::AddOverheadsAndPropagateEntries(global_graph.get());
  GraphProcessor::CalculateSizesForGraph(global_graph.get());

  // The same timestamp needs to be set for all dumps in the memory snapshot.
  base::TimeTicks timestamp = TRACE_TIME_TICKS_NOW();

  // Build up the global dump by iterating on the |valid| process dumps.
  mojom::GlobalMemoryDumpPtr global_dump(mojom::GlobalMemoryDump::New());
  global_dump->start_time = request->start_time;
  global_dump->process_dumps.reserve(request->responses.size());
  for (const auto& response : request->responses) {
    base::ProcessId pid = response.second.process_id;

    // On Linux, we may also have the browser process as a response.
    // Just ignore it if we are looking for the single pid case.
    if (request->args.pid != base::kNullProcessId && pid != request->args.pid)
      continue;

    // These pointers are owned by |request|.
    mojom::RawOSMemDump* raw_os_dump = pid_to_os_dump[pid];
    auto* raw_chrome_dump = pid_to_pmd[pid];

    // If we have the OS dump we should have the platform private footprint.
    DCHECK(!raw_os_dump || raw_os_dump->platform_private_footprint);

    // If the raw dump exists, create a summarised version of it.
    mojom::OSMemDumpPtr os_dump = nullptr;
    if (raw_os_dump) {
      uint64_t shared_resident_kb = 0;
#if BUILDFLAG(IS_MAC)
      // The resident, anonymous shared memory for each process is only
      // relevant on macOS.
      const auto process_graph_it =
          global_graph->process_node_graphs().find(pid);
      if (process_graph_it != global_graph->process_node_graphs().end()) {
        const auto& process_graph = process_graph_it->second;
        auto* node = process_graph->FindNode("shared_memory");
        if (node) {
          const auto& entry =
              node->entries()->find(RawMemoryGraphNode::kNameSize);
          if (entry != node->entries()->end())
            shared_resident_kb = entry->second.value_uint64 / 1024;
        }
      }
#endif
      os_dump = CreatePublicOSDump(
          *raw_os_dump, base::saturated_cast<uint32_t>(shared_resident_kb));
      os_dump->shared_footprint_kb =
          base::saturated_cast<uint32_t>(shared_footprints[pid] / 1024);
    }

    // Trace the OS and Chrome dumps if they exist.
    if (request->args.add_to_trace) {
      if (raw_os_dump) {
        bool trace_os_success = tracing_observer->AddOsDumpToTraceIfEnabled(
            request->GetRequestArgs(), pid, *os_dump, raw_os_dump->memory_maps,
            timestamp);
        if (!trace_os_success) {
          DLOG(ERROR) << "Tracing is disabled or not setup yet while receiving "
                         "OS dump for pid "
                      << pid;
          request->failed_memory_dump_count++;
        }
      }

      if (raw_chrome_dump) {
        bool trace_chrome_success = AddChromeMemoryDumpToTrace(
            request->GetRequestArgs(), pid, *raw_chrome_dump, tracing_observer,
            timestamp);
        if (!trace_chrome_success) {
          DLOG(ERROR) << "Tracing is disabled or not setup yet while receiving "
                         "Chrome dump for pid "
                      << pid;
          request->failed_memory_dump_count++;
        }
      }
    }

    bool valid = false;
    if (request->args.memory_footprint_only) {
      valid = raw_os_dump;
    } else {
      // Ignore incomplete results (can happen if the client
      // crashes/disconnects).
      valid = raw_os_dump && raw_chrome_dump &&
              (request->memory_map_option() == mojom::MemoryMapOption::NONE ||
               (raw_os_dump && !raw_os_dump->memory_maps.empty()));
    }

    if (!valid)
      continue;

    mojom::ProcessMemoryDumpPtr pmd = mojom::ProcessMemoryDump::New();
    pmd->pid = pid;
    pmd->process_type = pid_to_process_type[pid];
    pmd->os_dump = std::move(os_dump);
    pmd->service_name = response.second.service_name;

    // If we have to return a summary, add all entries for the requested
    // allocator dumps.
    if (request->should_return_summaries() &&
        !request->args.memory_footprint_only) {
      const auto& process_graph =
          global_graph->process_node_graphs().find(pid)->second;
      for (const std::string& name : request->args.allocator_dump_names) {
        bool is_recursive = base::EndsWith(name, "/*");
        std::string node_name =
            (is_recursive ? name.substr(0, name.length() - 2) : name);
        Node* node = process_graph->FindNode(node_name);

        // Silently ignore any missing node in the process graph.
        if (!node)
          continue;

        pmd->chrome_allocator_dumps.emplace(
            node_name, CreateAllocatorDumpForNode(node, is_recursive));
      }
    }

    global_dump->process_dumps.push_back(std::move(pmd));
  }
  global_dump->aggregated_metrics =
      ComputeGlobalNativeCodeResidentMemoryKb(pid_to_os_dump);

  const bool global_success = request->failed_memory_dump_count == 0;

  // In the single process-case, we want to ensure that global_success
  // is true if and only if global_dump is not nullptr.
  if (request->args.pid != base::kNullProcessId && !global_success) {
    global_dump = nullptr;
  }
  auto& callback = request->callback;
  std::move(callback).Run(global_success, request->dump_guid,
                          std::move(global_dump));

  char guid_str[20];
  snprintf(guid_str, sizeof(guid_str), "0x%" PRIx64, request->dump_guid);
  TRACE_EVENT_NESTABLE_ASYNC_END2(
      base::trace_event::MemoryDumpManager::kTraceCategory, "GlobalMemoryDump",
      TRACE_ID_LOCAL(request->dump_guid), "dump_guid", TRACE_STR_COPY(guid_str),
      "success", global_success);
}

bool QueuedRequestDispatcher::AddChromeMemoryDumpToTrace(
    const base::trace_event::MemoryDumpRequestArgs& args,
    base::ProcessId pid,
    const base::trace_event::ProcessMemoryDump& raw_chrome_dump,
    TracingObserver* tracing_observer,
    const base::TimeTicks& timestamp) {
  bool is_chrome_tracing_enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableChromeTracingComputation);
  if (!is_chrome_tracing_enabled) {
    return tracing_observer->AddChromeDumpToTraceIfEnabled(
        args, pid, &raw_chrome_dump, timestamp);
  }
  if (!tracing_observer->ShouldAddToTrace(args))
    return false;

  return tracing_observer->AddChromeDumpToTraceIfEnabled(
      args, pid, &raw_chrome_dump, timestamp);
}

QueuedRequestDispatcher::ClientInfo::ClientInfo(
    mojom::ClientProcess* client,
    base::ProcessId pid,
    mojom::ProcessType process_type,
    std::optional<std::string> service_name)
    : client(client),
      pid(pid),
      process_type(process_type),
      service_name(std::move(service_name)) {}

QueuedRequestDispatcher::ClientInfo::ClientInfo(ClientInfo&& other) = default;

QueuedRequestDispatcher::ClientInfo::~ClientInfo() = default;

}  // namespace memory_instrumentation
