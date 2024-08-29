// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/tracing/public/cpp/trace_event_args_allowlist.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/strings/pattern.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"

namespace tracing {
namespace {

// Each allowlist entry is used to allowlist an array arguments for a
// single or group of trace events.
struct AllowlistEntry {
  // Category name of the interested trace event.
  const char* category_name;
  // Pattern to match the interested trace event name.
  const char* event_name;
  // List of patterns that match the allowlisted arguments.
  // RAW_PTR_EXCLUSION: constant data that is not freed.
  RAW_PTR_EXCLUSION const char* const* arg_name_filter;
};

const char* const kScopedBlockingCallAllowedArgs[] = {
    "file_name", "function_name", "source_location", nullptr};
const char* const kPeekMessageAllowedArgs[] = {"chrome_message_pump", nullptr};
const char* const kMemoryDumpAllowedArgs[] = {
    "count", "dumps", "function", "top_queued_message_tag", nullptr};
const char* const kRendererHostAllowedArgs[] = {
    "class",           "line", "should_background", "has_pending_views",
    "bytes_allocated", nullptr};
const char* const kUIAllowedArgs[] = {"chrome_window_handle_event_info",
                                      nullptr};
const char* const kV8GCAllowedArgs[] = {"num_items", "num_tasks", nullptr};
const char* const kTopLevelIpcRunTaskAllowedArgs[] = {"chrome_task_annotator",
                                                      nullptr};
// TODO(ddrone): add args once creation_location_iid is parsed
const char* const kMemoryPressureEventsAllowedArgs[] = {
    "chrome_memory_pressure_notification", nullptr};

const AllowlistEntry kEventArgsAllowlist[] = {
    // Args recorded in perfetto protos and exported by trace processor JSON
    // exporter:

    {"__metadata", "thread_name", nullptr},
    {"__metadata", "process_name", nullptr},
    {"__metadata", "process_uptime_seconds", nullptr},
    {"base", "MemoryPressureListener::Notify",
     kMemoryPressureEventsAllowedArgs},
    {"base", "MessagePumpForUI::ProcessNextWindowsMessage PeekMessage",
     kPeekMessageAllowedArgs},
    {"base", "MultiSourceMemoryPressureMonitor::OnMemoryPressureLevelChanged",
     kMemoryPressureEventsAllowedArgs},
    {"base", "ScopedAllowBaseSyncPrimitivesOutsideBlockingScope",
     kScopedBlockingCallAllowedArgs},
    {"base", "ScopedAllowBlocking", kScopedBlockingCallAllowedArgs},
    {"base", "ScopedAllowIO", kScopedBlockingCallAllowedArgs},
    {"base", "ScopedBlockingCall*", kScopedBlockingCallAllowedArgs},
    {"base", "ScopedMayLoadLibraryAtBackgroundPriority",
     kScopedBlockingCallAllowedArgs},
    {"browser", "TabLoader::OnMemoryPressure",
     kMemoryPressureEventsAllowedArgs},
    {"browser", "KeyedServiceFactory::GetServiceForContext", nullptr},
    {"ipc", "GpuChannelHost::Send", nullptr},
    {"ipc", "SyncChannel::Send", nullptr},
    {"memory", "RenderThreadImpl::OnMemoryPressure",
     kMemoryPressureEventsAllowedArgs},
    {"shutdown", "*", nullptr},
    {"startup", "PrefProvider::PrefProvider", nullptr},
    {"startup", "TestAllowlist*", nullptr},
    {"toplevel", "*", nullptr},
    {"ui", "HWNDMessageHandler::OnWndProc", kUIAllowedArgs},
    {"ui", "HWNDMessageHandler::OnDwmCompositionChanged", kUIAllowedArgs},
    {TRACE_DISABLED_BY_DEFAULT("cpu_profiler"), "*", nullptr},
    {TRACE_DISABLED_BY_DEFAULT("lifecycles"), "task_posted_to_disabled_queue",
     nullptr},
    {TRACE_DISABLED_BY_DEFAULT("toplevel.ipc"), "TaskAnnotator::RunTask",
     kTopLevelIpcRunTaskAllowedArgs},
    {TRACE_DISABLED_BY_DEFAULT("user_action_samples"), "UserAction", nullptr},

    // Needs conversion to perfetto protos:

    {"__metadata", "chrome_library_address", nullptr},
    {"__metadata", "chrome_library_module", nullptr},
    {"__metadata", "stackFrames", nullptr},
    {"__metadata", "typeNames", nullptr},
    {"renderer_host", "*", kRendererHostAllowedArgs},
    // Redefined the string since MemoryDumpManager::kTraceCategory causes
    // static initialization of this struct.
    {TRACE_DISABLED_BY_DEFAULT("memory-infra"), "*", kMemoryDumpAllowedArgs},
    {TRACE_DISABLED_BY_DEFAULT("system_stats"), "*", nullptr},
    {TRACE_DISABLED_BY_DEFAULT("v8.gc"), "*", kV8GCAllowedArgs},
    {nullptr, nullptr, nullptr}};

const char* kMetadataAllowlist[] = {"chrome-bitness",
                                    "chrome-dcheck-on",
                                    "chrome-library-name",
                                    "clock-domain",
                                    "config",
                                    "cpu-*",
                                    "field-trials",
                                    "gpu-*",
                                    "highres-ticks",
                                    "hardware-class",
                                    "last_triggered_rule",
                                    "network-type",
                                    "num-cpus",
                                    "os-*",
                                    "physical-memory",
                                    "product-version",
                                    "scenario_name",
                                    "trace-config",
                                    "user-agent",
                                    nullptr};

}  // namespace

bool IsTraceArgumentNameAllowlisted(const char* const* granular_filter,
                                    const char* arg_name) {
  for (int i = 0; granular_filter[i] != nullptr; ++i) {
    if (base::MatchPattern(arg_name, granular_filter[i]))
      return true;
  }

  return false;
}

bool IsTraceEventArgsAllowlisted(
    const char* category_group_name,
    const char* event_name,
    base::trace_event::ArgumentNameFilterPredicate* arg_name_filter) {
  DCHECK(arg_name_filter);
  base::CStringTokenizer category_group_tokens(
      category_group_name, category_group_name + strlen(category_group_name),
      ",");
  while (category_group_tokens.GetNext()) {
    std::string_view category_group_token = category_group_tokens.token_piece();
    for (int i = 0; kEventArgsAllowlist[i].category_name != nullptr; ++i) {
      const AllowlistEntry& allowlist_entry = kEventArgsAllowlist[i];
      DCHECK(allowlist_entry.event_name);

      if (base::MatchPattern(category_group_token,
                             allowlist_entry.category_name) &&
          base::MatchPattern(event_name, allowlist_entry.event_name)) {
        if (allowlist_entry.arg_name_filter) {
          *arg_name_filter = base::BindRepeating(
              &IsTraceArgumentNameAllowlisted, allowlist_entry.arg_name_filter);
        }
        return true;
      }
    }
  }

  return false;
}

bool IsMetadataAllowlisted(const std::string& metadata_name) {
  for (size_t i = 0; kMetadataAllowlist[i] != nullptr; ++i) {
    if (base::MatchPattern(metadata_name, kMetadataAllowlist[i])) {
      return true;
    }
  }
  return false;
}

}  // namespace tracing
