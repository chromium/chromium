// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_TEST_FAKE_SPEECH_DISPATCHER_H_
#define CONTENT_BROWSER_SPEECH_TEST_FAKE_SPEECH_DISPATCHER_H_

#include <atomic>

struct FakeSpeechDispatcherState {
  std::atomic<int> open_calls{0};
  std::atomic<int> close_calls{0};
  std::atomic<int> say_calls{0};
  std::atomic<int> stop_calls{0};
  std::atomic<int> pause_calls{0};
  std::atomic<int> resume_calls{0};
  std::atomic<int> list_modules_calls{0};
  std::atomic<int> list_voices_calls{0};
  std::atomic<int> set_output_module_calls{0};
  std::atomic<int> set_synthesis_voice_calls{0};
  std::atomic<int> set_rate_calls{0};
  std::atomic<int> set_pitch_calls{0};
  std::atomic<int> set_language_calls{0};
  std::atomic<int> notifications{0};
  std::atomic<int> message_id{0};
  std::atomic<bool> finish_on_say{false};
};

#endif  // CONTENT_BROWSER_SPEECH_TEST_FAKE_SPEECH_DISPATCHER_H_
