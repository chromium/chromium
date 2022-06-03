// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/no_renderer_crashes_assertion.h"

#include "base/no_destructor.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// static
NoRendererCrashesAssertion::Suspensions&
NoRendererCrashesAssertion::Suspensions::GetInstance() {
  static base::NoDestructor<NoRendererCrashesAssertion::Suspensions>
      s_suspensions;
  return *s_suspensions;
}

NoRendererCrashesAssertion::Suspensions::Suspensions() = default;
NoRendererCrashesAssertion::Suspensions::~Suspensions() = default;

void NoRendererCrashesAssertion::Suspensions::AddSuspension(int process_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  process_id_to_suspension_count_[process_id]++;
}

void NoRendererCrashesAssertion::Suspensions::RemoveSuspension(int process_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = process_id_to_suspension_count_.find(process_id);
  DCHECK(it != process_id_to_suspension_count_.end());
  DCHECK_LT(0, it->second);
  --it->second;
  if (0 == it->second)
    process_id_to_suspension_count_.erase(it);
}

bool NoRendererCrashesAssertion::Suspensions::IsSuspended(int process_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, process_id);

  auto it = process_id_to_suspension_count_.find(process_id);
  if (it != process_id_to_suspension_count_.end() && it->second > 0)
    return true;

  auto it2 =
      process_id_to_suspension_count_.find(ChildProcessHost::kInvalidUniqueID);
  if (it2 != process_id_to_suspension_count_.end() && it2->second > 0)
    return true;

  return false;
}

NoRendererCrashesAssertion::NoRendererCrashesAssertion() {
  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 NotificationService::AllSources());
}

NoRendererCrashesAssertion::~NoRendererCrashesAssertion() = default;

void NoRendererCrashesAssertion::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type != NOTIFICATION_RENDERER_PROCESS_CLOSED)
    return;

  content::RenderProcessHost* process =
      content::Source<content::RenderProcessHost>(source).ptr();
  if (NoRendererCrashesAssertion::Suspensions::GetInstance().IsSuspended(
          process->GetID()))
    return;

  ChildProcessTerminationInfo* process_info =
      content::Details<content::ChildProcessTerminationInfo>(details).ptr();
  switch (process_info->status) {
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
    case base::TERMINATION_STATUS_STILL_RUNNING:
      return;  // Not a crash.
    default:
      break;  // Crash - need to trigger a test failure below.
  }

  const auto exit_code = process_info->exit_code;
  // Windows error codes such as 0xC0000005 and 0xC0000409 are much easier
  // to recognize and differentiate in hex.
  if (static_cast<int>(exit_code) < -100) {
    FAIL() << "Unexpected termination of a renderer process"
           << "; status: " << process_info->status << ", exit_code: 0x"
           << std::hex << exit_code;
  } else {
    // Print other error codes as a signed integer so that small negative
    // numbers are also recognizable.
    FAIL() << "Unexpected termination of a renderer process"
           << "; status: " << process_info->status
           << ", exit_code: " << exit_code;
  }
}

ScopedAllowRendererCrashes::ScopedAllowRendererCrashes()
    : ScopedAllowRendererCrashes(nullptr) {}

ScopedAllowRendererCrashes::ScopedAllowRendererCrashes(
    RenderProcessHost* process)
    : process_id_(process ? process->GetID()
                          : ChildProcessHost::kInvalidUniqueID) {
  NoRendererCrashesAssertion::Suspensions::GetInstance().AddSuspension(
      process_id_);
}

ScopedAllowRendererCrashes::ScopedAllowRendererCrashes(
    const ToRenderFrameHost& frame)
    : ScopedAllowRendererCrashes(frame.render_frame_host()->GetProcess()) {}

ScopedAllowRendererCrashes::~ScopedAllowRendererCrashes() {
  NoRendererCrashesAssertion::Suspensions::GetInstance().RemoveSuspension(
      process_id_);
}

}  // namespace content
