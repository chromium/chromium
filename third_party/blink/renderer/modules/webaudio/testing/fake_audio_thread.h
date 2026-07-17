// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_TESTING_FAKE_AUDIO_THREAD_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_TESTING_FAKE_AUDIO_THREAD_H_

#include <memory>

#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_type.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

class BaseAudioContext;

class FakeAudioThread {
 public:
  explicit FakeAudioThread(ThreadType thread_type = ThreadType::kTestThread);
  ~FakeAudioThread();

  FakeAudioThread(const FakeAudioThread&) = delete;
  FakeAudioThread& operator=(const FakeAudioThread&) = delete;

  // Run a closure on the fake audio thread and wait for it to complete.
  void RunOnAudioThread(CrossThreadOnceClosure closure);

  // Run a closure on the fake audio thread, and ensure the thread is registered
  // as the audio thread for the given context's DeferredTaskHandler before
  // running the closure.
  void RunOnAudioThreadWithContext(BaseAudioContext* context,
                                   CrossThreadOnceClosure closure);

  // Helper to get the underlying thread.
  NonMainThread* GetThread() const { return thread_.get(); }

 private:
  std::unique_ptr<NonMainThread> thread_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_TESTING_FAKE_AUDIO_THREAD_H_
