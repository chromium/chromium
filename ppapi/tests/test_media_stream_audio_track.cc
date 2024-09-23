// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tests PPB_MediaStreamAudioTrack interface.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/test_media_stream_audio_track.h"

// For MSVC.
#define _USE_MATH_DEFINES
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "ppapi/c/private/ppb_testing_private.h"
#include "ppapi/cpp/audio_buffer.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/var.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(MediaStreamAudioTrack);

namespace {

// Real constants defined in
// content/renderer/pepper/pepper_media_stream_audio_track_host.cc.
const int32_t kMaxNumberOfBuffers = 1000;
const int32_t kMinDuration = 10;
const int32_t kMaxDuration = 10000;
const int32_t kTimes = 3;
const char kJSCode[] =
    "function gotStream(stream) {"
    "  test_stream = stream;"
    "  var track = stream.getAudioTracks()[0];"
    "  var plugin = document.getElementById('plugin');"
    "  plugin.postMessage(track);"
    "}"
    "var constraints = {"
    "  audio: true,"
    "  video: false,"
    "};"
    "navigator.getUserMedia = "
    "    navigator.getUserMedia || navigator.webkitGetUserMedia;"
    "navigator.getUserMedia(constraints,"
    "    gotStream, function() {});";

const char kSineJSCode[] =
    // Create oscillators for the left and right channels. Use a sine wave,
    // which is the easiest to calculate expected values. The oscillator output
    // is low-pass filtered (as per spec) making comparison hard.
    "var context = new AudioContext();"
    "var l_osc = context.createOscillator();"
    "l_osc.type = \"sine\";"
    "l_osc.frequency.value = 25;"
    "var r_osc = context.createOscillator();"
    "r_osc.type = \"sine\";"
    "r_osc.frequency.value = 100;"
    // Combine the left and right channels.
    "var merger = context.createChannelMerger(2);"
    "merger.channelInterpretation = \"discrete\";"
    "l_osc.connect(merger, 0, 0);"
    "r_osc.connect(merger, 0, 1);"
    "var dest_stream = context.createMediaStreamDestination();"
    "merger.connect(dest_stream);"
    // Dump the generated waveform to a MediaStream output.
    "l_osc.start();"
    "r_osc.start();"
    "var track = dest_stream.stream.getAudioTracks()[0];"
    "var plugin = document.getElementById('plugin');"
    "plugin.postMessage(track);";

// Helper to check if the |sample_rate| is listed in PP_AudioBuffer_SampleRate
// enum.
bool IsSampleRateValid(PP_AudioBuffer_SampleRate sample_rate) {
  switch (sample_rate) {
    case PP_AUDIOBUFFER_SAMPLERATE_8000:
    case PP_AUDIOBUFFER_SAMPLERATE_16000:
    case PP_AUDIOBUFFER_SAMPLERATE_22050:
    case PP_AUDIOBUFFER_SAMPLERATE_32000:
    case PP_AUDIOBUFFER_SAMPLERATE_44100:
    case PP_AUDIOBUFFER_SAMPLERATE_48000:
    case PP_AUDIOBUFFER_SAMPLERATE_96000:
    case PP_AUDIOBUFFER_SAMPLERATE_192000:
      return true;
    default:
      return false;
  }
}

}  // namespace

TestMediaStreamAudioTrack::TestMediaStreamAudioTrack(TestingInstance* instance)
    : TestCase(instance),
      event_(instance_->pp_instance()) {
}

bool TestMediaStreamAudioTrack::Init() {
  return true;
}

TestMediaStreamAudioTrack::~TestMediaStreamAudioTrack() {
}

void TestMediaStreamAudioTrack::RunTests(const std::string& filter) {
  RUN_TEST(Create, filter);
  RUN_TEST(GetBuffer, filter);
  RUN_TEST(Configure, filter);
  RUN_TEST(ConfigureClose, filter);
  RUN_TEST(VerifyWaveform, filter);
}

void TestMediaStreamAudioTrack::HandleMessage(const pp::Var& message) {
  if (message.is_resource()) {
    audio_track_ = pp::MediaStreamAudioTrack(message.AsResource());
  }
  event_.Signal();
}

std::string TestMediaStreamAudioTrack::TestCreate() {
  // Create a track.
  instance_->EvalScript(kJSCode);
  event_.Wait();
  event_.Reset();

  ASSERT_FALSE(audio_track_.is_null());
  ASSERT_FALSE(audio_track_.HasEnded());
  ASSERT_FALSE(audio_track_.GetId().empty());

  // Close the track.
  audio_track_.Close();
  ASSERT_TRUE(audio_track_.HasEnded());
  audio_track_ = pp::MediaStreamAudioTrack();
  PASS();
}

std::string TestMediaStreamAudioTrack::TestGetBuffer() {
  // Create a track.
  instance_->EvalScript(kJSCode);
  event_.Wait();
  event_.Reset();

  ASSERT_FALSE(audio_track_.is_null());
  ASSERT_FALSE(audio_track_.HasEnded());
  ASSERT_FALSE(audio_track_.GetId().empty());

  PP_TimeDelta timestamp = 0.0;

  // Get |kTimes| buffers.
  for (int i = 0; i < kTimes; ++i) {
    TestCompletionCallbackWithOutput<pp::AudioBuffer> cc(
        instance_->pp_instance(), false);
    cc.WaitForResult(audio_track_.GetBuffer(cc.GetCallback()));
    ASSERT_EQ(PP_OK, cc.result());
    pp::AudioBuffer buffer = cc.output();
    ASSERT_FALSE(buffer.is_null());
    ASSERT_TRUE(IsSampleRateValid(buffer.GetSampleRate()));
    ASSERT_EQ(buffer.GetSampleSize(), PP_AUDIOBUFFER_SAMPLESIZE_16_BITS);

    ASSERT_GE(buffer.GetTimestamp(), timestamp);
    timestamp = buffer.GetTimestamp();

    ASSERT_GT(buffer.GetDataBufferSize(), 0U);
    ASSERT_TRUE(buffer.GetDataBuffer() != NULL);

    audio_track_.RecycleBuffer(buffer);

    // A recycled buffer should be invalidated.
    ASSERT_EQ(buffer.GetSampleRate(), PP_AUDIOBUFFER_SAMPLERATE_UNKNOWN);
    ASSERT_EQ(buffer.GetSampleSize(), PP_AUDIOBUFFER_SAMPLESIZE_UNKNOWN);
    ASSERT_EQ(buffer.GetDataBufferSize(), 0U);
    ASSERT_TRUE(buffer.GetDataBuffer() == NULL);
  }

  // Close the track.
  audio_track_.Close();
  ASSERT_TRUE(audio_track_.HasEnded());
  audio_track_ = pp::MediaStreamAudioTrack();
  PASS();
}

std::string TestMediaStreamAudioTrack::CheckConfigure(
    int32_t attrib_list[], int32_t expected_result) {
  TestCompletionCallback cc_configure(instance_->pp_instance(), false);
  cc_configure.WaitForResult(
      audio_track_.Configure(attrib_list, cc_configure.GetCallback()));
  ASSERT_EQ(expected_result, cc_configure.result());
  PASS();
}

std::string TestMediaStreamAudioTrack::CheckGetBuffer(
    int times, int expected_duration) {
  PP_TimeDelta timestamp = 0.0;
  for (int j = 0; j < times; ++j) {
    TestCompletionCallbackWithOutput<pp::AudioBuffer> cc_get_buffer(
        instance_->pp_instance(), false);
    cc_get_buffer.WaitForResult(
        audio_track_.GetBuffer(cc_get_buffer.GetCallback()));
    ASSERT_EQ(PP_OK, cc_get_buffer.result());
    pp::AudioBuffer buffer = cc_get_buffer.output();
    ASSERT_FALSE(buffer.is_null());
    ASSERT_TRUE(IsSampleRateValid(buffer.GetSampleRate()));
    ASSERT_EQ(buffer.GetSampleSize(), PP_AUDIOBUFFER_SAMPLESIZE_16_BITS);

    ASSERT_GE(buffer.GetTimestamp(), timestamp);
    timestamp = buffer.GetTimestamp();

    ASSERT_TRUE(buffer.GetDataBuffer() != NULL);
    if (expected_duration > 0) {
      uint32_t buffer_size = buffer.GetDataBufferSize();
      uint32_t channels = buffer.GetNumberOfChannels();
      uint32_t sample_rate = buffer.GetSampleRate();
      uint32_t bytes_per_frame = channels * 2;
      int32_t duration = expected_duration;
      ASSERT_EQ(buffer_size % bytes_per_frame, 0U);
      ASSERT_EQ(buffer_size,
                (duration * sample_rate * bytes_per_frame) / 1000);
    } else {
      ASSERT_GT(buffer.GetDataBufferSize(), 0U);
    }

    audio_track_.RecycleBuffer(buffer);
  }
  PASS();
}

std::string TestMediaStreamAudioTrack::TestConfigure() {
  // Create a track.
  instance_->EvalScript(kJSCode);
  event_.Wait();
  event_.Reset();

  ASSERT_FALSE(audio_track_.is_null());
  ASSERT_FALSE(audio_track_.HasEnded());
  ASSERT_FALSE(audio_track_.GetId().empty());

  // Perform a |Configure()| with no attributes. This ends up making an IPC
  // call, but the host implementation has a fast-path when there are no changes
  // to the configuration. This test is intended to hit that fast-path and make
  // sure it works correctly.
  {
    int32_t attrib_list[] = {
      PP_MEDIASTREAMAUDIOTRACK_ATTRIB_NONE,
    };
    ASSERT_SUBTEST_SUCCESS(CheckConfigure(attrib_list, PP_OK));
  }

  // Configure number of buffers.
  struct {
    int32_t buffers;
    int32_t expect_result;
  } buffers[] = {
    { 8, PP_OK },
    { 100, PP_OK },
    { kMaxNumberOfBuffers, PP_OK },
    { -1, PP_ERROR_BADARGUMENT },
    { kMaxNumberOfBuffers + 1, PP_OK },  // Clipped to max value.
    { 0, PP_OK },  // Use default.
  };
  for (size_t i = 0; i < sizeof(buffers) / sizeof(buffers[0]); ++i) {
    int32_t attrib_list[] = {
      PP_MEDIASTREAMAUDIOTRACK_ATTRIB_BUFFERS, buffers[i].buffers,
      PP_MEDIASTREAMAUDIOTRACK_ATTRIB_NONE,
    };
    ASSERT_SUBTEST_SUCCESS(CheckConfigure(attrib_list,
                                          buffers[i].expect_result));
    // Get some buffers. This should also succeed when configure fails.
    ASSERT_SUBTEST_SUCCESS(CheckGetBuffer(kTimes, -1));
  }

  // Configure buffer duration.
  struct {
    int32_t duration;
    int32_t expect_result;
  } durations[] = {
    { kMinDuration, PP_OK },
    { 123, PP_OK },
    { kMinDuration - 1, PP_ERROR_BADARGUMENT },
    { kMaxDuration + 1, PP_ERROR_BADARGUMENT },
  };
  for (size_t i = 0; i < sizeof(durations) / sizeof(durations[0]); ++i) {
    int32_t attrib_list[] = {
      PP_MEDIASTREAMAUDIOTRACK_ATTRIB_DURATION, durations[i].duration,
      PP_MEDIASTREAMAUDIOTRACK_ATTRIB_NONE,
    };
    ASSERT_SUBTEST_SUCCESS(CheckConfigure(attrib_list,
                                          durations[i].expect_result));

    // Get some buffers. This always works, but the buffer size will vary.
    int duration =
        durations[i].expect_result == PP_OK ? durations[i].duration : -1;
    ASSERT_SUBTEST_SUCCESS(CheckGetBuffer(kTimes, duration));
  }
  // Test kMaxDuration separately since each GetBuffer will take 10 seconds.
  {
    int32_t attrib_list[] = {
      PP_MEDIASTREAMAUDIOTRACK_ATTRIB_DURATION, kMaxDuration,
      PP_MEDIASTREAMAUDIOTRACK_ATTRIB_NONE,
    };
    ASSERT_SUBTEST_SUCCESS(CheckConfigure(attrib_list, PP_OK));
  }

  // Reset the duration to prevent the next part from taking 10 seconds.
  {
    int32_t attrib_list[] = {
      PP_MEDIASTREAMAUDIOTRACK_ATTRIB_DURATION, kMinDuration,
      PP_MEDIASTREAMAUDIOTRACK_ATTRIB_NONE,
    };
    ASSERT_SUBTEST_SUCCESS(CheckConfigure(attrib_list, PP_OK));
  }

  // Configure should fail while plugin holds buffers.
  {
    TestCompletionCallbackWithOutput<pp::AudioBuffer> cc_get_buffer(
        instance_->pp_instance(), false);
    cc_get_buffer.WaitForResult(
        audio_track_.GetBuffer(cc_get_buffer.GetCallback()));
    ASSERT_EQ(PP_OK, cc_get_buffer.result());
    pp::AudioBuffer buffer = cc_get_buffer.output();
    int32_t attrib_list[] = {
      PP_MEDIASTREAMAUDIOTRACK_ATTRIB_BUFFERS, 0,
      PP_MEDIASTREAMAUDIOTRACK_ATTRIB_NONE,
    };
    TestCompletionCallback cc_configure(instance_->pp_instance(), false);
    cc_configure.WaitForResult(
        audio_track_.Configure(attrib_list, cc_configure.GetCallback()));
    ASSERT_EQ(PP_ERROR_INPROGRESS, cc_configure.result());
    audio_track_.RecycleBuffer(buffer);
  }

  // Close the track.
  audio_track_.Close();
  ASSERT_TRUE(audio_track_.HasEnded());
  audio_track_ = pp::MediaStreamAudioTrack();
  PASS();
}

std::string TestMediaStreamAudioTrack::TestConfigureClose() {
  // Create a track.
  instance_->EvalScript(kJSCode);
  event_.Wait();
  event_.Reset();

  ASSERT_FALSE(audio_track_.is_null());
  ASSERT_FALSE(audio_track_.HasEnded());
  ASSERT_FALSE(audio_track_.GetId().empty());

  // Configure the audio track and close it immediately. The Configure() call
  // should complete.
  int32_t attrib_list[] = {
    PP_MEDIASTREAMAUDIOTRACK_ATTRIB_BUFFERS, 10,
    PP_MEDIASTREAMAUDIOTRACK_ATTRIB_NONE,
  };
  TestCompletionCallback cc_configure(instance_->pp_instance(), false);
  int32_t result = audio_track_.Configure(attrib_list,
                                          cc_configure.GetCallback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  audio_track_.Close();
  cc_configure.WaitForResult(result);
  result = cc_configure.result();
  // Unfortunately, we can't control whether the configure succeeds or is
  // aborted.
  ASSERT_TRUE(result == PP_OK || result == PP_ERROR_ABORTED);

  PASS();
}

uint32_t CalculateWaveStartingTime(int16_t sample, int16_t next_sample,
                                   uint32_t period) {
  int16_t slope = next_sample - sample;
  double angle = asin(sample / (double)INT16_MAX);
  if (slope < 0) {
    angle = M_PI - angle;
  }
  if (angle < 0) {
    angle += 2 * M_PI;
  }
  return round(angle * period / (2 * M_PI));
}

std::string TestMediaStreamAudioTrack::TestVerifyWaveform() {
  // Create a track.
  instance_->EvalScript(kSineJSCode);
  event_.Wait();
  event_.Reset();

  ASSERT_FALSE(audio_track_.is_null());
  ASSERT_FALSE(audio_track_.HasEnded());
  ASSERT_FALSE(audio_track_.GetId().empty());

  // Use a weird buffer length and number of buffers.
  const int32_t kBufferSize = 13;
  const int32_t kNumBuffers = 3;

  const uint32_t kChannels = 2;
  const uint32_t kFreqLeft = 25;
  const uint32_t kFreqRight = 100;

  int32_t attrib_list[] = {
    PP_MEDIASTREAMAUDIOTRACK_ATTRIB_DURATION, kBufferSize,
    PP_MEDIASTREAMAUDIOTRACK_ATTRIB_BUFFERS, kNumBuffers,
    PP_MEDIASTREAMAUDIOTRACK_ATTRIB_NONE,
  };
  ASSERT_SUBTEST_SUCCESS(CheckConfigure(attrib_list, PP_OK));

  // Get kNumBuffers buffers and verify they conform to the expected waveform.
  PP_TimeDelta timestamp = 0.0;
  int sample_time = 0;
  uint32_t left_start = 0;
  uint32_t right_start = 0;
  for (int j = 0; j < kNumBuffers; ++j) {
    TestCompletionCallbackWithOutput<pp::AudioBuffer> cc_get_buffer(
        instance_->pp_instance(), false);
    cc_get_buffer.WaitForResult(
        audio_track_.GetBuffer(cc_get_buffer.GetCallback()));
    ASSERT_EQ(PP_OK, cc_get_buffer.result());
    pp::AudioBuffer buffer = cc_get_buffer.output();
    ASSERT_FALSE(buffer.is_null());
    ASSERT_TRUE(IsSampleRateValid(buffer.GetSampleRate()));
    ASSERT_EQ(buffer.GetSampleSize(), PP_AUDIOBUFFER_SAMPLESIZE_16_BITS);
    ASSERT_EQ(buffer.GetNumberOfChannels(), kChannels);
    ASSERT_GE(buffer.GetTimestamp(), timestamp);
    timestamp = buffer.GetTimestamp();

    uint32_t buffer_size = buffer.GetDataBufferSize();
    uint32_t sample_rate = buffer.GetSampleRate();
    uint32_t num_samples = buffer.GetNumberOfSamples();
    uint32_t bytes_per_frame = kChannels * 2;
    ASSERT_EQ(num_samples, (kChannels * kBufferSize * sample_rate) / 1000);
    ASSERT_EQ(buffer_size % bytes_per_frame, 0U);
    ASSERT_EQ(buffer_size, num_samples * 2);

    // Period of sine wave, in samples.
    uint32_t left_period = sample_rate / kFreqLeft;
    uint32_t right_period = sample_rate / kFreqRight;

    int16_t* data_buffer = static_cast<int16_t*>(buffer.GetDataBuffer());
    ASSERT_TRUE(data_buffer != NULL);

    if (j == 0) {
      // The generated wave doesn't necessarily start at 0, so compensate for
      // this.
      left_start = CalculateWaveStartingTime(data_buffer[0], data_buffer[2],
                                             left_period);
      right_start = CalculateWaveStartingTime(data_buffer[1], data_buffer[3],
                                              right_period);
    }

    for (uint32_t sample = 0; sample < num_samples;
         sample += 2, sample_time++) {
      int16_t left = data_buffer[sample];
      int16_t right = data_buffer[sample + 1];
      double angle = (2.0 * M_PI * ((sample_time + left_start) % left_period)) /
          left_period;
      int16_t expected = INT16_MAX * sin(angle);
      // Account for off-by-one errors due to rounding.
      ASSERT_GE(left, std::max<int16_t>(expected, INT16_MIN + 1) - 1);
      ASSERT_LE(left, std::min<int16_t>(expected, INT16_MAX - 1) + 1);

      angle = (2 * M_PI * ((sample_time + right_start) % right_period)) /
          right_period;
      expected = INT16_MAX * sin(angle);
      ASSERT_GE(right, std::max<int16_t>(expected, INT16_MIN + 1) - 1);
      ASSERT_LE(right, std::min<int16_t>(expected, INT16_MAX - 1) + 1);
    }

    audio_track_.RecycleBuffer(buffer);
  }

  PASS();
}
