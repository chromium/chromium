// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/crash_report_utils.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/numerics/checked_math.h"
#include "base/process/kill.h"
#include "base/process/memory.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/document_associated_data.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/common/result_codes.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/base/isolation_info.h"
#include "partition_alloc/page_allocator.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"

#if BUILDFLAG(IS_POSIX)
#include <signal.h>
#include <sys/wait.h>
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

namespace content::crash_report {

void InitializeContext(
    RenderFrameHostImpl& rfh,
    uint64_t length,
    base::OnceCallback<void(base::UnsafeSharedMemoryRegion)> callback) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kCrashReportingStorageAPI)) {
    mojo::ReportBadMessage("kCrashReportingStorageAPI feature must be enabled");
    return;
  }

  if (length > blink::mojom::kMaxCrashReportContextSize) {
    bad_message::ReceivedBadMessage(
        rfh.GetProcess(), bad_message::RFH_CRASH_REPORT_STORAGE_SIZE_TOO_LARGE);
    return;
  }

  // A renderer process must not try and create and initialize its shared memory
  // buffer for crash reports more than once. See
  // `CrashReportContext::initialize()`.
  if (rfh.document_associated_data().crash_report_storage_region().IsValid()) {
    bad_message::ReceivedBadMessage(
        rfh.GetProcess(),
        bad_message::RFH_CRASH_REPORT_STORAGE_ALREADY_INITIALIZED);
    return;
  }

  // The shared memory buffer that the browser allocates needs to be as many
  // bytes as the renderer requests, plus `sizeof(uint32_t)` bytes for a leading
  // integer that describes how many bytes of the buffer are used/written to.
  //
  // Every time a value is added or removed from the shared memory, this leading
  // integer is updated by the renderer, and the browser process finally uses it
  // in `ReadAPIBody()` when parsing the bytes.
  base::CheckedNumeric<uint32_t> total_size = length;
  total_size += sizeof(uint32_t);

  // We can use `ValueOrDie()` below because we know `length` is sufficiently
  // far enough from the boundary of `uint32_t` (because of the
  // `kMaxCrashReportContextSize` check) such that it cannot have overflown at
  // this point.
  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(total_size.ValueOrDie());
  if (!region.IsValid()) {
    std::move(callback).Run(base::UnsafeSharedMemoryRegion());
    return;
  }

  rfh.document_associated_data().SetCrashReportContextRegion(
      region.Duplicate());
  std::move(callback).Run(std::move(region));
}

base::DictValue ReadAPIBody(const base::UnsafeSharedMemoryRegion& region) {
  base::DictValue crash_report_api_body;
  if (!region.IsValid()) {
    return crash_report_api_body;
  }

  // Create a writable mapping that we only read from; we cannot create a
  // read-only mapping from an unsafe shared memory region, since the
  // read-only mapping has no way to guarantee that the platform handle
  // backing the unsafe region is truly no longer writable.
  base::WritableSharedMemoryMapping mapping = region.Map();

  if (!mapping.IsValid() || mapping.size() < sizeof(uint32_t)) {
    return crash_report_api_body;
  }

  base::SpanReader<const uint8_t> reader(
      mapping.GetMemoryAsSpan<const uint8_t>());
  // The first uint32_t-sized chunk of the memory tells us how large the
  // *remaining* amount of data that we need to read is.
  uint32_t data_size = 0;
  reader.ReadU32NativeEndian(data_size);

  // Only attempt to read the report data if the recorded size of the data
  // fits inside the real size of the shared memory. Only a compromised
  // renderer could set us up to fail this condition; see
  // `CrashReportContext::set()`, which ensures that the writes of oversized
  // reports are not attempted.
  if (mapping.size() < sizeof(data_size) + data_size) {
    return crash_report_api_body;
  }

  // Take a copy of the entire crash report API memory, to snapshot the data so
  // that a malicious writer can't change it while it's being parsed. Note that
  // a malicious writer could technically change the data that we're about to
  // snapshot, after we've read `data_size`, but changing the contents or format
  // of this data after we snapshot the body below shouldn't matter; the worst
  // that can happen is the data is no longer valid JSON, and it gets omitted
  // from the report.
  std::string body_copy(data_size, '\0');
  const bool read_success =
      reader.ReadCopy(base::as_writable_bytes(base::span(body_copy)));
  CHECK(read_success);
  // We use `JSON_PARSE_RFC` here, because the writer uses
  // `JSONValue::ToJSONString()` which outputs RFC-compliant JSON.
  std::optional<base::Value> parsed_json = base::JSONReader::Read(
      body_copy, base::JSONParserOptions::JSON_PARSE_RFC);

  if (parsed_json && parsed_json->is_dict()) {
    for (auto it : parsed_json->GetDict()) {
      if (it.second.is_string()) {
        crash_report_api_body.Set(it.first, it.second.GetString());
      }
    }
  }

  return crash_report_api_body;
}

std::optional<std::string> DetermineReason(
    RenderProcessHost& process,
    const ChildProcessTerminationInfo& info) {
  const int exit_code = info.exit_code;
  auto metrics = process.GetCrashMemoryMetrics();

  const bool is_oom =
      (metrics && metrics->allocation_failed != 0) ||
#if BUILDFLAG(IS_WIN)
      static_cast<DWORD>(exit_code) == base::win::kOomExceptionCode ||
      static_cast<DWORD>(exit_code) == base::win::kSandboxFatalMemoryExceeded ||
#endif
      exit_code ==
          static_cast<int>(partition_alloc::kTerminateOnCommitFailureExitCode);

  // All frames in the same renderer process share the main thread and V8
  // isolate, so if one frame hangs, every frame in the process is blocked and
  // killed together. A single `RenderProcessHost` can host multiple
  // `RenderWidgetHostImpl`s (e.g. sibling cross-origin subframes from the same
  // site, an A1 -> B -> A2 frame tree, or multiple tabs/popups sharing a
  // process), and input ack or commit timeouts only mark the single targeted
  // `RenderWidgetHostImpl` as unresponsive. Check whether any widget in the
  // process is unresponsive so that all active frames in the hung process
  // receive `reason: "unresponsive"`.
  bool is_unresponsive =
      exit_code == RESULT_CODE_HUNG ||
      !process.GetUnresponsiveDocumentJavascriptCallStack().empty();
  if (!is_unresponsive) {
    process.ForEachRenderFrameHost([&is_unresponsive](RenderFrameHost* rfh) {
      if (rfh->GetRenderWidgetHost() &&
          rfh->GetRenderWidgetHost()->IsCurrentlyUnresponsive()) {
        is_unresponsive = true;
      }
    });
  }

  // Check the termination status to see if a crash occurred (and potentially
  // determine the |reason| for the crash). Note: |is_oom| takes precedence over
  // |is_unresponsive| because a renderer exhausting memory often becomes
  // unresponsive prior to termination.
  std::string reason;
  switch (info.status) {
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
#if BUILDFLAG(IS_WIN)
    case base::TERMINATION_STATUS_INTEGRITY_FAILURE:
#endif
      if (is_oom) {
        reason = "oom";
      } else if (is_unresponsive) {
        reason = "unresponsive";
      }
      break;
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      if (is_oom) {
        reason = "oom";
      } else if (is_unresponsive) {
        reason = "unresponsive";
      } else if (exit_code == RESULT_CODE_KILLED_BAD_MESSAGE
#if BUILDFLAG(IS_POSIX)
                 || (WIFSIGNALED(exit_code) && WTERMSIG(exit_code) == SIGKILL)
#endif
      ) {
        // On POSIX, bad-message kills, kernel OOM kills, and
        // GetKnownDeadTerminationStatus() races terminate the process with
        // SIGKILL (mapping to TERMINATION_STATUS_PROCESS_WAS_KILLED). Report
        // these as generic crashes with no reason.
      } else {
        // A kill that is neither OOM, unresponsive, nor a forced SIGKILL /
        // bad-message termination (such as a manual kill via the Task Manager,
        // which sends SIGTERM on POSIX or RESULT_CODE_KILLED on Windows) is an
        // intentional termination and should not be reported.
        return std::nullopt;
      }
      break;
    case base::TERMINATION_STATUS_OOM:
    case base::TERMINATION_STATUS_EVICTED_FOR_MEMORY:
      reason = "oom";
      break;
#if BUILDFLAG(IS_ANDROID)
    case base::TERMINATION_STATUS_OOM_PROTECTED:
      // On Android, child processes are spawned from the system Zygote rather
      // than being direct children of the browser process, so waitpid() does
      // not work and ChildProcessLauncherHelper::GetTerminationInfo() maps all
      // foreground process terminations to TERMINATION_STATUS_OOM_PROTECTED
      // (while background/waived processes—including routine OS memory
      // evictions—receive TERMINATION_STATUS_NORMAL_TERMINATION and are not
      // reported).
      if (process.FastShutdownStarted() || process.IsDeletingSoon() ||
          info.clean_exit) {
        return std::nullopt;
      }
      if (is_oom) {
        reason = "oom";
      } else if (is_unresponsive) {
        reason = "unresponsive";
      } else if (info.was_killed_intentionally_by_browser ||
                 process.ShutdownRequested()) {
        if (exit_code != RESULT_CODE_KILLED_BAD_MESSAGE) {
          // An intentional browser-initiated shutdown or Task Manager kill
          // should not be reported.
          return std::nullopt;
        }
      }
      break;
#endif
#if BUILDFLAG(IS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
      // On ChromeOS, GetTerminationStatus() maps any SIGKILL to
      // TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM, even when the process was
      // killed by the browser (e.g. via TerminateInternal() for a bad IPC
      // message, or via GetKnownDeadTerminationStatus() while reaping an
      // unresponsive or shutting-down renderer). Check |is_oom|,
      // |is_unresponsive|, and |exit_code| before attributing the kill to OOM.
      if (is_oom) {
        reason = "oom";
      } else if (is_unresponsive) {
        reason = "unresponsive";
      } else if (exit_code == RESULT_CODE_KILLED_BAD_MESSAGE) {
        // Report bad-message kills as generic crashes with no reason.
      } else if (exit_code == RESULT_CODE_NORMAL_EXIT ||
                 exit_code == RESULT_CODE_KILLED) {
        // An intentional browser-initiated shutdown or Task Manager kill that
        // was reaped with SIGKILL should not be reported.
        return std::nullopt;
      } else {
        reason = "oom";
      }
      break;
#endif
    default:
      // Other termination statuses do not indicate a crash.
      return std::nullopt;
  }

  return reason;
}

void MaybeGenerate(RenderFrameHostImpl& rfh,
                   const ChildProcessTerminationInfo& info) {
  if (!rfh.MarkCrashReportAttempted()) {
    return;
  }

  if (!rfh.GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    return;
  }

  // Only generate crash reports for active documents (and not documents in
  // BackForwardCache, Prerendering, PendingCommit, Speculative, or
  // PendingDeletion states). When a renderer process terminates in those
  // states, the browser silently evicts the BFCached page, cancels
  // prerendering, or completes frame deletion in the background; the user is
  // not interacting with the document and never observes a crash.
  if (!rfh.IsActive()) {
    return;
  }

  RenderProcessHost* process = rfh.GetProcess();
  std::optional<std::string> reason = DetermineReason(*process, info);
  if (!reason.has_value()) {
    return;
  }

  // Construct the crash report.
  base::DictValue body;
  if (base::FeatureList::IsEnabled(
          blink::features::kCrashReportingAPIMoreContextData)) {
    body.Set("is_top_level", rfh.IsOutermostMainFrame());
    body.Set("visibility_state",
             rfh.GetVisibilityState() == PageVisibilityState::kVisible
                 ? "visible"
                 : "hidden");
  }

  // Read the key/value data from the document associated data shared memory
  // buffer that the renderer may have written to; parse it as JSON, and add it
  // to the crash report.
  base::DictValue crash_report_api_body =
      ReadAPIBody(rfh.document_associated_data().crash_report_storage_region());
  body.Set("crash_report_api", std::move(crash_report_api_body));

  if (!reason->empty()) {
    body.Set("reason", *reason);
  }

  if (*reason == "unresponsive" &&
      base::FeatureList::IsEnabled(
          blink::features::kDocumentPolicyIncludeJSCallStacksInCrashReports)) {
    const std::string& unresponsive_document_javascript_call_stack =
        process->GetUnresponsiveDocumentJavascriptCallStack();
    const blink::LocalFrameToken& unresponsive_document_token =
        process->GetUnresponsiveDocumentToken();

    if (!unresponsive_document_javascript_call_stack.empty()) {
      if (unresponsive_document_token == rfh.GetFrameToken()) {
        body.Set("stack", unresponsive_document_javascript_call_stack);
      } else {
        body.Set("stack", "Unable to collect JS call stack.");
      }
    }
  }

  // Send the crash report to the Reporting API.
  process->GetStoragePartition()->GetNetworkContext()->QueueReport(
      /*type=*/"crash", rfh.crash_reporting_group(), rfh.GetLastCommittedURL(),
      rfh.GetReportingSource(),
      rfh.GetIsolationInfoForSubresources().network_anonymization_key(),
      std::move(body));
}

}  // namespace content::crash_report
