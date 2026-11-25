// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_CRASH_REPORT_UTILS_H_
#define CONTENT_BROWSER_RENDERER_HOST_CRASH_REPORT_UTILS_H_

#include <cstdint>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "content/common/content_export.h"

namespace base {
class UnsafeSharedMemoryRegion;
}  // namespace base

namespace content {

struct ChildProcessTerminationInfo;
class RenderFrameHostImpl;
class RenderProcessHost;

namespace crash_report {

// Reporting API endpoint group name used when the `Reporting-Endpoints`
// response header explicitly defines a `crash-reporting` endpoint.
inline constexpr char kGroupName[] = "crash-reporting";

// Allocates and initializes the per-document shared memory buffer backing the
// CrashReportStorage API (`window.crashReport`) for `rfh`.
CONTENT_EXPORT void InitializeContext(
    RenderFrameHostImpl& rfh,
    uint64_t length,
    base::OnceCallback<void(base::UnsafeSharedMemoryRegion)> callback);

// Reads and parses the key/value crash report context written by the renderer
// into `region` as RFC-compliant JSON, returning a dictionary of string entries
// (or an empty dictionary if unset, invalid, or malformed).
CONTENT_EXPORT base::DictValue ReadAPIBody(
    const base::UnsafeSharedMemoryRegion& region);

// Determines whether `info` for `process` represents a reportable renderer
// crash. Returns `std::nullopt` if no crash report should be generated (e.g.
// normal exit or intentional browser/user kill), or the crash reason string
// (`"oom"`, `"unresponsive"`, or `""` for a generic crash with no specific
// reason).
CONTENT_EXPORT std::optional<std::string> DetermineReason(
    RenderProcessHost& process,
    const ChildProcessTerminationInfo& info);

// Generates and queues a Crash Reporting API report for `rfh` if `rfh` has not
// already reported a crash for its current RenderFrame, is an active HTTP(S)
// document, and `info` indicates a reportable crash.
CONTENT_EXPORT void MaybeGenerate(RenderFrameHostImpl& rfh,
                                  const ChildProcessTerminationInfo& info);

}  // namespace crash_report
}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_CRASH_REPORT_UTILS_H_
