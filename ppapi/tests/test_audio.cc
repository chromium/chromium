// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/test_audio.h"

#include <stddef.h>
#include <string.h>

#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

#if defined(__native_client__)
#include "native_client/src/untrusted/irt/irt.h"
#include "ppapi/native_client/src/untrusted/irt_stub/thread_creator.h"
#endif

#define ARRAYSIZE_UNSAFE(a) \
  ((sizeof(a) / sizeof(*(a))) / \
   static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))

#if defined(__native_client__)
namespace {

void GetNaClIrtPpapiHook(struct nacl_irt_ppapihook* hooks) {
  nacl_interface_query(NACL_IRT_PPAPIHOOK_v0_1, hooks, sizeof(*hooks));
}

struct PP_ThreadFunctions g_thread_funcs = {};

void ThreadFunctionsGetter(const struct PP_ThreadFunctions* thread_funcs) {
  g_thread_funcs = *thread_funcs;
}

// In order to check if the thread_create is called, CountingThreadCreate()
// increments this variable. Callers can check if the function is actually
// called by looking at this value.
int g_num_thread_create_called = 0;
int g_num_thread_join_called = 0;

int CountingThreadCreate(uintptr_t* tid,
                         void (*func)(void* thread_argument),
                         void* thread_argument) {
  ++g_num_thread_create_called;
  return g_thread_funcs.thread_create(tid, func, thread_argument);
}

int CountingThreadJoin(uintptr_t tid) {
  ++g_num_thread_join_called;
  return g_thread_funcs.thread_join(tid);
}

// Sets NULL for PP_ThreadFunctions to emulate the situation that
// ppapi_register_thread_creator() is not yet called.
void SetNullThreadFunctions() {
  nacl_irt_ppapihook hooks;
  GetNaClIrtPpapiHook(&hooks);
  PP_ThreadFunctions thread_functions = {};
  hooks.ppapi_register_thread_creator(&thread_functions);
}

void InjectCountingThreadFunctions() {
  // First of all, we extract the system default thread functions.
  // Internally, __nacl_register_thread_creator calls
  // hooks.ppapi_register_thread_creator with default PP_ThreadFunctions
  // instance. ThreadFunctionGetter stores it to g_thread_funcs.
  nacl_irt_ppapihook hooks = { NULL, ThreadFunctionsGetter };
  __nacl_register_thread_creator(&hooks);

  // Here g_thread_funcs stores the thread functions.
  // Inject the CountingThreadCreate.
  PP_ThreadFunctions thread_functions = {
    CountingThreadCreate,
    CountingThreadJoin,
  };
  GetNaClIrtPpapiHook(&hooks);
  hooks.ppapi_register_thread_creator(&thread_functions);
}

// Resets the PP_ThreadFunctions on exit from the scope.
class ScopedThreadFunctionsResetter {
 public:
  ScopedThreadFunctionsResetter() {}
  ~ScopedThreadFunctionsResetter() {
    nacl_irt_ppapihook hooks;
    GetNaClIrtPpapiHook(&hooks);
    __nacl_register_thread_creator(&hooks);
  }
};

}  // namespace
#endif  // __native_client__

REGISTER_TEST_CASE(Audio);

TestAudio::TestAudio(TestingInstance* instance)
    : TestCase(instance),
      audio_callback_method_(NULL),
      audio_callback_event_(instance->pp_instance()),
      test_done_(false),
      audio_interface_(NULL),
      audio_interface_1_0_(NULL),
      audio_config_interface_(NULL),
      core_interface_(NULL) {
}

TestAudio::~TestAudio() {
}

bool TestAudio::Init() {
  audio_interface_ = static_cast<const PPB_Audio_1_1*>(
      pp::Module::Get()->GetBrowserInterface(PPB_AUDIO_INTERFACE_1_1));
  audio_interface_1_0_ = static_cast<const PPB_Audio_1_0*>(
      pp::Module::Get()->GetBrowserInterface(PPB_AUDIO_INTERFACE_1_0));
  audio_config_interface_ = static_cast<const PPB_AudioConfig*>(
      pp::Module::Get()->GetBrowserInterface(PPB_AUDIO_CONFIG_INTERFACE));
  core_interface_ = static_cast<const PPB_Core*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CORE_INTERFACE));
  return audio_interface_ && audio_interface_1_0_ && audio_config_interface_ &&
         core_interface_;
}

void TestAudio::RunTests(const std::string& filter) {
  RUN_TEST(Creation, filter);
  RUN_TEST(DestroyNoStop, filter);
  RUN_TEST(Failures, filter);
  RUN_TEST(AudioCallback1, filter);
  RUN_TEST(AudioCallback2, filter);
  RUN_TEST(AudioCallback3, filter);
  RUN_TEST(AudioCallback4, filter);

#if defined(__native_client__)
  RUN_TEST(AudioThreadCreatorIsRequired, filter);
  RUN_TEST(AudioThreadCreatorIsCalled, filter);
#endif
}

// Test creating audio resources for all guaranteed sample rates and various
// frame counts.
std::string TestAudio::TestCreation() {
  static const PP_AudioSampleRate kSampleRates[] = {
    PP_AUDIOSAMPLERATE_44100,
    PP_AUDIOSAMPLERATE_48000
  };
  static const uint32_t kRequestFrameCounts[] = {
    PP_AUDIOMINSAMPLEFRAMECOUNT,
    PP_AUDIOMAXSAMPLEFRAMECOUNT,
    // Include some "okay-looking" frame counts; check their validity below.
    PP_AUDIOSAMPLERATE_44100 / 100,  // 10ms @ 44.1kHz
    PP_AUDIOSAMPLERATE_48000 / 100,  // 10ms @ 48kHz
    2 * PP_AUDIOSAMPLERATE_44100 / 100,  // 20ms @ 44.1kHz
    2 * PP_AUDIOSAMPLERATE_48000 / 100,  // 20ms @ 48kHz
    1024,
    2048,
    4096
  };
  PP_AudioSampleRate sample_rate = audio_config_interface_->RecommendSampleRate(
      instance_->pp_instance());
  ASSERT_TRUE(sample_rate == PP_AUDIOSAMPLERATE_NONE ||
              sample_rate == PP_AUDIOSAMPLERATE_44100 ||
              sample_rate == PP_AUDIOSAMPLERATE_48000);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kSampleRates); i++) {
    sample_rate = kSampleRates[i];

    for (size_t j = 0; j < ARRAYSIZE_UNSAFE(kRequestFrameCounts); j++) {
      // Make a config, create the audio resource, and release the config.
      uint32_t request_frame_count = kRequestFrameCounts[j];
      uint32_t frame_count = audio_config_interface_->RecommendSampleFrameCount(
          instance_->pp_instance(), sample_rate, request_frame_count);
      PP_Resource ac = audio_config_interface_->CreateStereo16Bit(
          instance_->pp_instance(), sample_rate, frame_count);
      ASSERT_TRUE(ac);
      PP_Resource audio = audio_interface_->Create(
          instance_->pp_instance(), ac, AudioCallbackTrampoline, this);
      core_interface_->ReleaseResource(ac);
      ac = 0;

      ASSERT_TRUE(audio);
      ASSERT_TRUE(audio_interface_->IsAudio(audio));

      // Check that the config returned for |audio| matches what we gave it.
      ac = audio_interface_->GetCurrentConfig(audio);
      ASSERT_TRUE(ac);
      ASSERT_TRUE(audio_config_interface_->IsAudioConfig(ac));
      ASSERT_EQ(sample_rate, audio_config_interface_->GetSampleRate(ac));
      ASSERT_EQ(frame_count, audio_config_interface_->GetSampleFrameCount(ac));
      core_interface_->ReleaseResource(ac);
      ac = 0;

      // Start and stop audio playback. The documentation indicates that
      // |StartPlayback()| and |StopPlayback()| may fail, but gives no
      // indication as to why ... so check that they succeed.
      audio_callback_method_ = &TestAudio::AudioCallbackTrivial;
      ASSERT_TRUE(audio_interface_->StartPlayback(audio));
      ASSERT_TRUE(audio_interface_->StopPlayback(audio));
      audio_callback_method_ = NULL;

      core_interface_->ReleaseResource(audio);
    }
  }

  PASS();
}

// Test that releasing the resource without calling |StopPlayback()| "works".
std::string TestAudio::TestDestroyNoStop() {
  PP_Resource ac = CreateAudioConfig(PP_AUDIOSAMPLERATE_44100, 2048);
  ASSERT_TRUE(ac);
  audio_callback_method_ = NULL;
  PP_Resource audio = audio_interface_->Create(
      instance_->pp_instance(), ac, AudioCallbackTrampoline, this);
  core_interface_->ReleaseResource(ac);
  ac = 0;

  ASSERT_TRUE(audio);
  ASSERT_TRUE(audio_interface_->IsAudio(audio));

  // Start playback and release the resource.
  audio_callback_method_ = &TestAudio::AudioCallbackTrivial;
  ASSERT_TRUE(audio_interface_->StartPlayback(audio));
  core_interface_->ReleaseResource(audio);
  audio_callback_method_ = NULL;

  PASS();
}

std::string TestAudio::TestFailures() {
  // Test invalid parameters to |Create()|.

  // We want a valid config for some of our tests of |Create()|.
  PP_Resource ac = CreateAudioConfig(PP_AUDIOSAMPLERATE_44100, 2048);
  ASSERT_TRUE(ac);

  // Failure cases should never lead to the callback being called.
  audio_callback_method_ = NULL;

  // Invalid instance -> failure.
  PP_Resource audio = audio_interface_->Create(
      0, ac, AudioCallbackTrampoline, this);
  ASSERT_EQ(0, audio);

  // Invalid config -> failure.
  audio = audio_interface_->Create(
      instance_->pp_instance(), 0, AudioCallbackTrampoline, this);
  ASSERT_EQ(0, audio);

  // Null callback -> failure.
  audio = audio_interface_->Create(
      instance_->pp_instance(), ac, NULL, NULL);
  ASSERT_EQ(0, audio);

  core_interface_->ReleaseResource(ac);
  ac = 0;

  // Test the other functions with an invalid audio resource.
  ASSERT_FALSE(audio_interface_->IsAudio(0));
  ASSERT_EQ(0, audio_interface_->GetCurrentConfig(0));
  ASSERT_FALSE(audio_interface_->StartPlayback(0));
  ASSERT_FALSE(audio_interface_->StopPlayback(0));

  PASS();
}

// NOTE: |TestAudioCallbackN| assumes that the audio callback is called at least
// once. If the audio stream does not start up correctly or is interrupted this
// may not be the case and these tests will fail. However, in order to properly
// test the audio callbacks, we must have a configuration where audio can
// successfully play, so we assume this is the case on bots.

// This test starts playback and verifies that:
//  1) the audio callback is actually called;
//  2) that |StopPlayback()| waits for the audio callback to finish.
std::string TestAudio::TestAudioCallback1() {
  PP_Resource ac = CreateAudioConfig(PP_AUDIOSAMPLERATE_44100, 1024);
  ASSERT_TRUE(ac);
  audio_callback_method_ = NULL;
  PP_Resource audio = audio_interface_->Create(
      instance_->pp_instance(), ac, AudioCallbackTrampoline, this);
  core_interface_->ReleaseResource(ac);
  ac = 0;

  audio_callback_event_.Reset();
  test_done_ = false;

  audio_callback_method_ = &TestAudio::AudioCallbackTest;
  ASSERT_TRUE(audio_interface_->StartPlayback(audio));

  // Wait for the audio callback to be called.
  audio_callback_event_.Wait();
  ASSERT_TRUE(audio_interface_->StopPlayback(audio));
  test_done_ = true;

  // If any more audio callbacks are generated, we should crash (which is good).
  audio_callback_method_ = NULL;

  core_interface_->ReleaseResource(audio);

  PASS();
}

// This is the same as |TestAudioCallback1()|, except that instead of calling
// |StopPlayback()|, it just releases the resource.
std::string TestAudio::TestAudioCallback2() {
  PP_Resource ac = CreateAudioConfig(PP_AUDIOSAMPLERATE_44100, 1024);
  ASSERT_TRUE(ac);
  audio_callback_method_ = NULL;
  PP_Resource audio = audio_interface_->Create(
      instance_->pp_instance(), ac, AudioCallbackTrampoline, this);
  core_interface_->ReleaseResource(ac);
  ac = 0;

  audio_callback_event_.Reset();
  test_done_ = false;

  audio_callback_method_ = &TestAudio::AudioCallbackTest;
  ASSERT_TRUE(audio_interface_->StartPlayback(audio));

  // Wait for the audio callback to be called.
  audio_callback_event_.Wait();

  core_interface_->ReleaseResource(audio);

  test_done_ = true;

  // If any more audio callbacks are generated, we should crash (which is good).
  audio_callback_method_ = NULL;

  PASS();
}

// This is the same as |TestAudioCallback1()|, except that it attempts a second
// round of |StartPlayback| and |StopPlayback| to make sure the callback
// function still responds when using the same audio resource.
std::string TestAudio::TestAudioCallback3() {
  PP_Resource ac = CreateAudioConfig(PP_AUDIOSAMPLERATE_44100, 1024);
  ASSERT_TRUE(ac);
  audio_callback_method_ = NULL;
  PP_Resource audio = audio_interface_->Create(
      instance_->pp_instance(), ac, AudioCallbackTrampoline, this);
  core_interface_->ReleaseResource(ac);
  ac = 0;

  audio_callback_event_.Reset();
  test_done_ = false;

  audio_callback_method_ = &TestAudio::AudioCallbackTest;
  ASSERT_TRUE(audio_interface_->StartPlayback(audio));

  // Wait for the audio callback to be called.
  audio_callback_event_.Wait();

  ASSERT_TRUE(audio_interface_->StopPlayback(audio));

  // Repeat one more |StartPlayback| & |StopPlayback| cycle, and verify again
  // that the callback function was invoked.
  audio_callback_event_.Reset();
  ASSERT_TRUE(audio_interface_->StartPlayback(audio));

  // Wait for the audio callback to be called.
  audio_callback_event_.Wait();
  ASSERT_TRUE(audio_interface_->StopPlayback(audio));
  test_done_ = true;

  // If any more audio callbacks are generated, we should crash (which is good).
  audio_callback_method_ = NULL;

  core_interface_->ReleaseResource(audio);

  PASS();
}

// This is the same as |TestAudioCallback1()|, except that it uses
// PPB_Audio_1_0.
std::string TestAudio::TestAudioCallback4() {
  PP_Resource ac = CreateAudioConfig(PP_AUDIOSAMPLERATE_44100, 1024);
  ASSERT_TRUE(ac);
  audio_callback_method_ = NULL;
  PP_Resource audio = audio_interface_1_0_->Create(
      instance_->pp_instance(), ac, AudioCallbackTrampoline1_0, this);
  core_interface_->ReleaseResource(ac);
  ac = 0;

  audio_callback_event_.Reset();
  test_done_ = false;

  audio_callback_method_ = &TestAudio::AudioCallbackTest;
  ASSERT_TRUE(audio_interface_1_0_->StartPlayback(audio));

  // Wait for the audio callback to be called.
  audio_callback_event_.Wait();
  ASSERT_TRUE(audio_interface_1_0_->StopPlayback(audio));
  test_done_ = true;

  // If any more audio callbacks are generated, we should crash (which is good).
  audio_callback_method_ = NULL;

  core_interface_->ReleaseResource(audio);

  PASS();
}

#if defined(__native_client__)
// Tests the behavior of the thread_create functions.
// For PPB_Audio_Shared to work properly, the user code must call
// ppapi_register_thread_creator(). This test checks the error handling for the
// case when user code doesn't call ppapi_register_thread_creator().
std::string TestAudio::TestAudioThreadCreatorIsRequired() {
  // We'll inject some thread functions in this test case.
  // Reset them at the end of this case.
  ScopedThreadFunctionsResetter thread_resetter;

  // Set the thread functions to NULLs to emulate the situation where
  // ppapi_register_thread_creator() is not called by user code.
  SetNullThreadFunctions();

  PP_Resource ac = CreateAudioConfig(PP_AUDIOSAMPLERATE_44100, 1024);
  ASSERT_TRUE(ac);
  audio_callback_method_ = NULL;
  PP_Resource audio = audio_interface_->Create(
      instance_->pp_instance(), ac, AudioCallbackTrampoline, this);
  core_interface_->ReleaseResource(ac);
  ac = 0;

  // StartPlayback() fails, because no thread creating function
  // is available.
  ASSERT_FALSE(audio_interface_->StartPlayback(audio));

  // If any more audio callbacks are generated,
  // we should crash (which is good).
  audio_callback_method_ = NULL;

  core_interface_->ReleaseResource(audio);

  PASS();
}

// Tests whether the thread functions passed from the user code are actually
// called.
std::string TestAudio::TestAudioThreadCreatorIsCalled() {
  // We'll inject some thread functions in this test case.
  // Reset them at the end of this case.
  ScopedThreadFunctionsResetter thread_resetter;

  // Inject the thread counting function. In the injected function,
  // when called, g_num_thread_create_called is incremented.
  g_num_thread_create_called = 0;
  g_num_thread_join_called = 0;
  InjectCountingThreadFunctions();

  PP_Resource ac = CreateAudioConfig(PP_AUDIOSAMPLERATE_44100, 1024);
  ASSERT_TRUE(ac);
  audio_callback_method_ = NULL;
  PP_Resource audio = audio_interface_->Create(
      instance_->pp_instance(), ac, AudioCallbackTrampoline, this);
  core_interface_->ReleaseResource(ac);
  ac = 0;

  audio_callback_event_.Reset();
  test_done_ = false;

  audio_callback_method_ = &TestAudio::AudioCallbackTest;
  ASSERT_TRUE(audio_interface_->StartPlayback(audio));

  // Wait for the audio callback to be called.
  audio_callback_event_.Wait();
  // Here, the injected thread_create is called, but thread_join is not yet.
  ASSERT_EQ(1, g_num_thread_create_called);
  ASSERT_EQ(0, g_num_thread_join_called);

  ASSERT_TRUE(audio_interface_->StopPlayback(audio));

  test_done_ = true;

  // Here, the injected thread_join is called.
  ASSERT_EQ(1, g_num_thread_join_called);

  // If any more audio callbacks are generated,
  // we should crash (which is good).
  audio_callback_method_ = NULL;

  core_interface_->ReleaseResource(audio);

  PASS();
}
#endif

// TODO(raymes): Test that actually playback happens correctly, etc.

static void Crash() {
  *static_cast<volatile unsigned*>(NULL) = 0xdeadbeef;
}

// static
void TestAudio::AudioCallbackTrampoline(void* sample_buffer,
                                        uint32_t buffer_size_in_bytes,
                                        PP_TimeDelta latency,
                                        void* user_data) {
  TestAudio* thiz = static_cast<TestAudio*>(user_data);

  // Crash if on the main thread.
  if (thiz->core_interface_->IsMainThread())
    Crash();

  AudioCallbackMethod method = thiz->audio_callback_method_;
  (thiz->*method)(sample_buffer, buffer_size_in_bytes, latency);
}

// static
void TestAudio::AudioCallbackTrampoline1_0(void* sample_buffer,
                                           uint32_t buffer_size_in_bytes,
                                           void* user_data) {
  AudioCallbackTrampoline(sample_buffer, buffer_size_in_bytes, 0.0, user_data);
}

void TestAudio::AudioCallbackTrivial(void* sample_buffer,
                                     uint32_t buffer_size_in_bytes,
                                     PP_TimeDelta latency) {
  if (latency < 0)
    Crash();

  memset(sample_buffer, 0, buffer_size_in_bytes);
}

void TestAudio::AudioCallbackTest(void* sample_buffer,
                                  uint32_t buffer_size_in_bytes,
                                  PP_TimeDelta latency) {
  if (test_done_ || latency < 0)
    Crash();

  memset(sample_buffer, 0, buffer_size_in_bytes);
  audio_callback_event_.Signal();
}

PP_Resource TestAudio::CreateAudioConfig(
    PP_AudioSampleRate sample_rate,
    uint32_t requested_sample_frame_count) {
  uint32_t frame_count = audio_config_interface_->RecommendSampleFrameCount(
      instance_->pp_instance(), sample_rate, requested_sample_frame_count);
  return audio_config_interface_->CreateStereo16Bit(
      instance_->pp_instance(), sample_rate, frame_count);
}
