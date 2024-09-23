// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "media/base/eme_constants.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "media/base/pipeline_status.h"
#include "media/base/test_data_util.h"
#include "media/media_buildflags.h"
#include "media/test/pipeline_integration_test_base.h"
#include "media/test/test_media_source.h"
#include "third_party/googletest/src/googletest/src/gtest-internal-inl.h"

namespace {

// Keep these aligned with BUILD.gn's pipeline_integration_fuzzer_variants
enum FuzzerVariant {
  SRC,
  WEBM_OPUS,
  WEBM_VORBIS,
  WEBM_VP8,
  WEBM_VP9,
  WEBM_OPUS_VP9,
#if BUILDFLAG(ENABLE_AV1_DECODER)
  MP4_AV1,
#endif
  MP4_FLAC,
  MP4_OPUS,
  MP3,
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  ADTS,
  MP4_AACLC,
  MP4_AACSBR,
  MP4_AVC1,
  MP4_AACLC_AVC,
#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
  MP2T_AACLC,
  MP2T_AACSBR,
  MP2T_AVC,
  MP2T_MP3,
  MP2T_AACLC_AVC,
#endif  // BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
};

std::string MseFuzzerVariantEnumToMimeTypeString(FuzzerVariant variant) {
  switch (variant) {
    case WEBM_OPUS:
      return "audio/webm; codecs=\"opus\"";
    case WEBM_VORBIS:
      return "audio/webm; codecs=\"vorbis\"";
    case WEBM_VP8:
      return "video/webm; codecs=\"vp8\"";
    case WEBM_VP9:
      return "video/webm; codecs=\"vp9\"";
    case WEBM_OPUS_VP9:
      return "video/webm; codecs=\"opus,vp9\"";
#if BUILDFLAG(ENABLE_AV1_DECODER)
    case MP4_AV1:
      return "video/mp4; codecs=\"av01.0.04M.08\"";
#endif  // BUILDFLAG(ENABLE_AV1_DECODER)
    case MP4_FLAC:
      return "audio/mp4; codecs=\"flac\"";
    case MP4_OPUS:
      return "audio/mp4; codecs=\"opus\"";
    case MP3:
      return "audio/mpeg";
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    case ADTS:
      return "audio/aac";
    case MP4_AACLC:
      return "audio/mp4; codecs=\"mp4a.40.2\"";
    case MP4_AACSBR:
      return "audio/mp4; codecs=\"mp4a.40.5\"";
    case MP4_AVC1:
      return "video/mp4; codecs=\"avc1.42E01E\"";
    case MP4_AACLC_AVC:
      return "video/mp4; codecs=\"mp4a.40.2,avc1.42E01E\"";
#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
    case MP2T_AACLC:
      return "video/mp2t; codecs=\"mp4a.67\"";
    case MP2T_AACSBR:
      return "video/mp2t; codecs=\"mp4a.40.5\"";
    case MP2T_AVC:
      return "video/mp2t; codecs=\"avc1.42E01E\"";
    case MP2T_MP3:
      // Note, "mp4a.6B" appears to be an equivalent codec substring.
      return "video/mp2t; codecs=\"mp4a.69\"";
    case MP2T_AACLC_AVC:
      return "video/mp2t; codecs=\"mp4a.40.2,avc1.42E01E\"";
#endif  // BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

    case SRC:
      NOTREACHED_IN_MIGRATION() << "SRC is an invalid MSE fuzzer variant";
      break;
  }

  return "";
}

}  // namespace

namespace media {

// Limit the amount of initial (or post-seek) audio silence padding allowed in
// rendering of fuzzed input.
constexpr base::TimeDelta kMaxPlayDelay = base::Seconds(10);

void OnEncryptedMediaInitData(media::PipelineIntegrationTestBase* test,
                              media::EmeInitDataType /* type */,
                              const std::vector<uint8_t>& /* init_data */) {
  // Encrypted media is not supported in this test. For an encrypted media file,
  // we will start demuxing the data but media pipeline will wait for a CDM to
  // be available to start initialization, which will not happen in this case.
  // To prevent the test timeout, we'll just fail the test immediately here.
  // Note: Since the callback is on the media task runner but the test is on
  // the main task runner, this must be posted.
  // TODO(xhwang): Support encrypted media in this fuzzer test.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PipelineIntegrationTestBase::FailTest,
                                base::Unretained(test),
                                media::PIPELINE_ERROR_INITIALIZATION_FAILED));
}

void OnAudioPlayDelay(media::PipelineIntegrationTestBase* test,
                      base::TimeDelta play_delay) {
  CHECK_GT(play_delay, base::TimeDelta());
  if (play_delay > kMaxPlayDelay) {
    // Note: Since the callback is on the media task runner but the test is on
    // the main task runner, this must be posted.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&PipelineIntegrationTestBase::FailTest,
                                  base::Unretained(test),
                                  media::PIPELINE_ERROR_INITIALIZATION_FAILED));
  }
}

class ProgressivePipelineIntegrationFuzzerTest
    : public PipelineIntegrationTestBase {
 public:
  ProgressivePipelineIntegrationFuzzerTest() {
    set_encrypted_media_init_data_cb(
        base::BindRepeating(&OnEncryptedMediaInitData, this));
    set_audio_play_delay_cb(base::BindPostTaskToCurrentDefault(
        base::BindRepeating(&OnAudioPlayDelay, this)));
  }

  ~ProgressivePipelineIntegrationFuzzerTest() override = default;

  void RunTest(const uint8_t* data, size_t size) {
    if (PIPELINE_OK != Start(data, size, kUnreliableDuration | kFuzzing))
      return;

    Play();
    if (PIPELINE_OK != WaitUntilEndedOrError())
      return;

    Seek(base::TimeDelta());
  }
};

class MediaSourcePipelineIntegrationFuzzerTest
    : public PipelineIntegrationTestBase {
 public:
  MediaSourcePipelineIntegrationFuzzerTest() {
    set_encrypted_media_init_data_cb(
        base::BindRepeating(&OnEncryptedMediaInitData, this));
    set_audio_play_delay_cb(base::BindPostTaskToCurrentDefault(
        base::BindRepeating(&OnAudioPlayDelay, this)));
  }

  ~MediaSourcePipelineIntegrationFuzzerTest() override = default;

  void RunTest(const uint8_t* data, size_t size, const std::string& mimetype) {
    if (size == 0)
      return;

    auto external_memory =
        std::make_unique<media::ExternalMemoryAdapterForTesting>(
            base::make_span(data, size));
    scoped_refptr<media::DecoderBuffer> buffer =
        media::DecoderBuffer::FromExternalMemory(std::move(external_memory));

    TestMediaSource source(buffer, mimetype, kAppendWholeFile);

    // Prevent timeout in the case of not enough media appended to complete
    // demuxer initialization, yet no error in the media appended.  The
    // following will trigger DEMUXER_ERROR_COULD_NOT_OPEN state transition in
    // this case.
    source.set_do_eos_after_next_append(true);

    source.set_encrypted_media_init_data_cb(
        base::BindRepeating(&OnEncryptedMediaInitData, this));

    // Allow parsing to either pass or fail without emitting a gtest failure
    // from TestMediaSource.
    source.set_expected_append_result(
        TestMediaSource::ExpectedAppendResult::kSuccessOrFailure);

    // TODO(wolenetz): Vary the behavior (abort/remove/seek/endOfStream/Append
    // in pieces/append near play-head/vary append mode/etc), perhaps using
    // CustomMutator and Seed to insert/update the variation information into/in
    // the |data| we process here.  See https://crbug.com/750818.
    // Use |kFuzzing| test type to allow pipeline start to either pass or fail
    // without emitting a gtest failure.
    if (PIPELINE_OK != StartPipelineWithMediaSource(&source, kFuzzing, nullptr))
      return;

    Play();
  }
};

}  // namespace media

// Disable noisy logging.
struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);

    // |test| instances uses TaskEnvironment, which needs TestTimeouts.
    TestTimeouts::Initialize();

    media::InitializeMediaLibrary();

    // Note, instead of LOGGING_FATAL, use a value at or below
    // logging::LOGGING_VERBOSE here to assist local debugging.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
  }
};

Environment* env = new Environment();

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Media pipeline starts new threads, which needs AtExitManager.
  base::AtExitManager at_exit;

  FuzzerVariant variant = PIPELINE_FUZZER_VARIANT;

  // These tests use GoogleTest assertions without using the GoogleTest
  // framework. While this is the case, tell GoogleTest's stack trace getter
  // that GoogleTest is being left now so that there is a basis for traces
  // collected upon assertion failure. TODO(crbug.com/40113640): use
  // RUN_ALL_TESTS() and remove this code.
  ::testing::internal::GetUnitTestImpl()
      ->os_stack_trace_getter()
      ->UponLeavingGTest();
  if (variant == SRC) {
    media::ProgressivePipelineIntegrationFuzzerTest test;
    test.RunTest(data, size);
  } else {
    media::MediaSourcePipelineIntegrationFuzzerTest test;
    test.RunTest(data, size, MseFuzzerVariantEnumToMimeTypeString(variant));
  }

  return 0;
}
