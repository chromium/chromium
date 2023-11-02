// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_AUDIO_H_
#define PPAPI_TESTS_TEST_AUDIO_H_

#include <stdint.h>

#include <string>

#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/tests/test_case.h"

class TestAudio : public TestCase {
 public:
  explicit TestAudio(TestingInstance* instance);
  ~TestAudio();

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestCreation();
  std::string TestDestroyNoStop();
  std::string TestFailures();
  std::string TestAudioCallback1();
  std::string TestAudioCallback2();
  std::string TestAudioCallback3();
  std::string TestAudioCallback4();

#if defined(__native_client__)
  std::string TestAudioThreadCreatorIsRequired();
  std::string TestAudioThreadCreatorIsCalled();
#endif

  // Calls |audio_callback_method_| (where |user_data| is "this").
  static void AudioCallbackTrampoline(void* sample_buffer,
                                      uint32_t buffer_size_in_bytes,
                                      PP_TimeDelta latency,
                                      void* user_data);
  static void AudioCallbackTrampoline1_0(void* sample_buffer,
                                         uint32_t buffer_size_in_bytes,
                                         void* user_data);

  typedef void (TestAudio::*AudioCallbackMethod)(void* sample_buffer,
                                                 uint32_t buffer_size_in_bytes,
                                                 PP_TimeDelta latency);

  // Method called by |AudioCallbackTrampoline()|. Set only when the callback
  // can't be running (before |StartPlayback()|, after |StopPlayback()| or
  // releasing the last reference to the audio resource).
  AudioCallbackMethod audio_callback_method_;

  // An |AudioCallbackMethod| that just clears |sample_buffer|.
  void AudioCallbackTrivial(void* sample_buffer,
                            uint32_t buffer_size_in_bytes,
                            PP_TimeDelta latency);

  // |AudioCallbackMethod| used by |TestAudioCallbackN()|.
  void AudioCallbackTest(void* sample_buffer,
                         uint32_t buffer_size_in_bytes,
                         PP_TimeDelta latency);

  PP_Resource CreateAudioConfig(PP_AudioSampleRate sample_rate,
                                uint32_t requested_sample_frame_count);

  // Used by |TestAudioCallbackN()|.
  NestedEvent audio_callback_event_;

  bool test_done_;

  // Raw C-level interfaces, set in |Init()|; do not modify them elsewhere.
  const PPB_Audio_1_1* audio_interface_;
  const PPB_Audio_1_0* audio_interface_1_0_;
  const PPB_AudioConfig* audio_config_interface_;
  const PPB_Core* core_interface_;
};

#endif  // PPAPI_TESTS_TEST_AUDIO_H_
