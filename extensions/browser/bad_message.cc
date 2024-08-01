// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/bad_message.h"

#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/browser/render_process_host.h"

namespace extensions {
namespace bad_message {

namespace {

void LogBadMessage(BadMessageReason reason) {
  LOG(ERROR) << "Terminating extension renderer for bad IPC message, reason "
             << reason;
  base::UmaHistogramSparse("Stability.BadMessageTerminated.Extensions", reason);
}

base::debug::CrashKeyString* GetBadMessageCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "extension_bad_message_reason", base::debug::CrashKeySize::Size64);
  return crash_key;
}

}  // namespace

void ReceivedBadMessage(content::RenderProcessHost* host,
                        BadMessageReason reason) {
  base::debug::ScopedCrashKeyString crash_key(GetBadMessageCrashKey(),
                                              base::NumberToString(reason));
  LogBadMessage(reason);
  host->ShutdownForBadMessage(
      content::RenderProcessHost::CrashReportMode::GENERATE_CRASH_DUMP);
}

void ReceivedBadMessage(int render_process_id, BadMessageReason reason) {
  content::RenderProcessHost* rph =
      content::RenderProcessHost::FromID(render_process_id);
  // The render process was already terminated.
  if (!rph) {
    return;
  }

  ReceivedBadMessage(rph, reason);
}

}  // namespace bad_message
}  // namespace extensions
