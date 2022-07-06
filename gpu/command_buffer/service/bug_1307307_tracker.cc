// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/bug_1307307_tracker.h"
#include "base/check.h"
#include "base/debug/crash_logging.h"
#include "base/debug/leak_annotations.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_local.h"

namespace gpu {

namespace {
base::ThreadLocalPointer<Bug1307307Tracker::VideoAccessError>&
LastThreadLocalVideoAccessError() {
  static base::NoDestructor<
      base::ThreadLocalPointer<Bug1307307Tracker::VideoAccessError>>
      last_video_access_error;
  return *last_video_access_error;
}
}  // namespace

Bug1307307Tracker::Bug1307307Tracker() = default;
Bug1307307Tracker::~Bug1307307Tracker() = default;

void Bug1307307Tracker::BeforeAccess() {
  ClearLastAccessError();
}

void Bug1307307Tracker::CopySubTextureFinished(const gpu::Mailbox& source,
                                               const gpu::Mailbox& destination,
                                               bool failed) {
  auto error = GetLastAccessError();
  DCHECK(error == VideoAccessError::kNoError || failed);

  auto it = copy_sub_texture_results_.Get(destination);
  if (it == copy_sub_texture_results_.end()) {
    it = copy_sub_texture_results_.Put(destination, CopySubTextureResult());
    it->second.ever_succeeded = !failed;
  }
  it->second.source = source;
  it->second.failed = failed;
  it->second.video_error = error;
  it->second.ever_succeeded |= !failed;
}

void Bug1307307Tracker::GenerateCrashKey(int hops,
                                         VideoAccessError video_error,
                                         bool cleared) {
  static auto* const cleared_crash_key = base::debug::AllocateCrashKeyString(
      "si-error-cleared", base::debug::CrashKeySize::Size32);
  static auto* const video_error_crash_key =
      base::debug::AllocateCrashKeyString("si-error-video",
                                          base::debug::CrashKeySize::Size32);

  base::debug::SetCrashKeyString(
      video_error_crash_key,
      base::StringPrintf("%d:%d", static_cast<int>(video_error), hops));
  base::debug::SetCrashKeyString(cleared_crash_key, cleared ? "1" : "0");
}

void Bug1307307Tracker::AccessFailed(const gpu::Mailbox& mailbox,
                                     bool cleared) {
  // Check if this mailbox had video access error.
  auto error = GetLastAccessError();
  if (error != VideoAccessError::kNoError) {
    GenerateCrashKey(0, error, cleared);
    return;
  }

  // There could be intermediate copies on the path from video decoder to the
  // canvas. If there is chain of copies: "video => image1 => image2 => canvas"
  // and the first one will fail in CopySubTexture call, the remaining will also
  // fail because intermediate images won't be marked as Cleared.

  // Traverse the CopySubTexture chain to find first CopySubTexture operation
  // that failed with VideoAccessError if any.
  auto it = copy_sub_texture_results_.Peek(mailbox);
  int hops = 1;
  while (it != copy_sub_texture_results_.end()) {
    if (it->second.video_error != VideoAccessError::kNoError) {
      GenerateCrashKey(hops, it->second.video_error, cleared);
      return;
    }

    it = copy_sub_texture_results_.Peek(it->second.source);
    hops++;
    // There is no guarantee that there is no loop, so bail out after
    // reasonable amount of iterations.
    if (hops > 10)
      break;
  }
  GenerateCrashKey(-1, VideoAccessError::kNoError, cleared);
}

void Bug1307307Tracker::SetLastAccessError(VideoAccessError error) {
  auto& last_error = LastThreadLocalVideoAccessError();
  if (!last_error.Get()) {
    auto* value = new VideoAccessError;
    // We never delete this pointer, but we should have only two threads (Gpu
    // Main and DrDc) that can call this function, so amount of memory is
    // capped.
    ANNOTATE_LEAKING_OBJECT_PTR(value);
    last_error.Set(value);
  }
  *last_error.Get() = error;
}

void Bug1307307Tracker::ClearLastAccessError() {
  SetLastAccessError(VideoAccessError::kNoError);
}

Bug1307307Tracker::VideoAccessError Bug1307307Tracker::GetLastAccessError() {
  if (auto* error = LastThreadLocalVideoAccessError().Get())
    return *error;
  return VideoAccessError::kNoError;
}
}  // namespace gpu
