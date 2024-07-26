// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_TEST_PIPELINE_INTEGRATION_TEST_BASE_H_
#define MEDIA_TEST_PIPELINE_INTEGRATION_TEST_BASE_H_

#include <stdint.h>

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/hash/md5.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/audio/clockless_audio_sink.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/demuxer.h"
#include "media/base/media_switches.h"
#include "media/base/mock_media_log.h"
#include "media/base/null_video_sink.h"
#include "media/base/pipeline_impl.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_frame.h"
#include "media/renderers/audio_renderer_impl.h"
#include "media/renderers/video_renderer_impl.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif  // BUILDFLAG(IS_WIN)

using ::testing::NiceMock;

namespace base {
class RunLoop;
}

namespace media {

class FakeEncryptedMedia;
class TestMediaSource;

// Empty MD5 hash string.  Used to verify empty video tracks.
extern const char kNullVideoHash[];

// Empty hash string.  Used to verify empty audio tracks.
extern const char kNullAudioHash[];

// Integration tests for Pipeline. Real demuxers, real decoders, and
// base renderer implementations are used to verify pipeline functionality. The
// renderers used in these tests rely heavily on the AudioRendererBase &
// VideoRendererImpl implementations which contain a majority of the code used
// in the real AudioRendererImpl & PaintCanvasVideoRenderer implementations used
// in the browser. The renderers in this test don't actually write data to a
// display or audio device. Both of these devices are simulated since they have
// little effect on verifying pipeline behavior and allow tests to run faster
// than real-time.
class PipelineIntegrationTestBase : public Pipeline::Client {
 public:
  PipelineIntegrationTestBase();

  PipelineIntegrationTestBase(const PipelineIntegrationTestBase&) = delete;
  PipelineIntegrationTestBase& operator=(const PipelineIntegrationTestBase&) =
      delete;

  virtual ~PipelineIntegrationTestBase();

  // Test types for advanced testing and benchmarking (e.g., underflow is
  // disabled to ensure consistent hashes). May be combined using the bitwise
  // or operator (and as such must have values that are powers of two).
  enum TestTypeFlags {
    kNormal = 0,
    kHashed = 1,
    kNoClockless = 2,
    kExpectDemuxerFailure = 4,
    kUnreliableDuration = 8,
    kWebAudio = 16,
    kMonoOutput = 32,
    kFuzzing = 64,
  };

  // Setup method to initialize various state according to flags.
  void ParseTestTypeFlags(uint8_t flags);

  // Starts the pipeline with a file specified by |filename|, optionally with a
  // CdmContext or a |test_type|, returning the final status code after it has
  // started. |filename| points at a test file located under media/test/data/.
  PipelineStatus Start(const std::string& filename);
  PipelineStatus Start(const std::string& filename, CdmContext* cdm_context);
  PipelineStatus Start(
      const std::string& filename,
      uint8_t test_type,
      CreateVideoDecodersCB prepend_video_decoders_cb = CreateVideoDecodersCB(),
      CreateAudioDecodersCB prepend_audio_decoders_cb =
          CreateAudioDecodersCB());

  // Starts the pipeline with |data| (with |size| bytes). The |data| will be
  // valid throughtout the lifetime of this test.
  PipelineStatus Start(const uint8_t* data, size_t size, uint8_t test_type);

  void Play();
  void Pause();
  bool Seek(base::TimeDelta seek_time);
  bool Suspend();
  bool Resume(base::TimeDelta seek_time);
  void Stop();

  // Fails the test with |status|.
  void FailTest(PipelineStatus status);

  bool WaitUntilCurrentTimeIsAfter(const base::TimeDelta& wait_time);
  bool WaitUntilOnEnded();
  PipelineStatus WaitUntilEndedOrError();

  // Returns the MD5 hash of all video frames seen.  Should only be called once
  // after playback completes.  First time hashes should be generated with
  // --video-threads=1 to ensure correctness.  Pipeline must have been started
  // with hashing enabled.
  std::string GetVideoHash();

  // Returns the hash of all audio frames seen.  Should only be called once
  // after playback completes.  Pipeline must have been started with hashing
  // enabled.
  const AudioHash& GetAudioHash() const;

  // Reset video hash to restart hashing from scratch (e.g. after a seek or
  // after disabling a media track).
  void ResetVideoHash();

  // Returns the time taken to render the complete audio file.
  // Pipeline must have been started with clockless playback enabled.
  base::TimeDelta GetAudioTime();

  // Sets a callback to handle EME "encrypted" event. Must be called to test
  // potentially encrypted media.
  void set_encrypted_media_init_data_cb(
      const Demuxer::EncryptedMediaInitDataCB& encrypted_media_init_data_cb) {
    encrypted_media_init_data_cb_ = encrypted_media_init_data_cb;
  }

  // Saves a test callback, ownership of which will be transferred to the next
  // AudioRendererImpl created by CreateRendererImpl().
  void set_audio_play_delay_cb(AudioRendererImpl::PlayDelayCBForTesting cb) {
    audio_play_delay_cb_ = std::move(cb);
  }

  std::unique_ptr<Renderer> CreateRenderer(
      std::optional<RendererType> renderer_type);

 protected:
  base::test::ScopedFeatureList scoped_feature_list_{kBuiltInH264Decoder};
  NiceMock<MockMediaLog> media_log_;
  base::test::TaskEnvironment task_environment_;
  base::MD5Context md5_context_;
  bool hashing_enabled_;
  bool clockless_playback_;
  bool webaudio_attached_;
  bool mono_output_;
  bool fuzzing_;
#if defined(ADDRESS_SANITIZER) || defined(UNDEFINED_SANITIZER)
  // TODO(crbug.com/40610469): ASAN causes Run() timeouts to be reached.
  const base::test::ScopedDisableRunLoopTimeout disable_run_timeout_;
#endif
  std::unique_ptr<Demuxer> demuxer_;
  std::unique_ptr<DataSource> data_source_;
  std::unique_ptr<PipelineImpl> pipeline_;
  scoped_refptr<NullAudioSink> audio_sink_;
  scoped_refptr<ClocklessAudioSink> clockless_audio_sink_;
  std::unique_ptr<NullVideoSink> video_sink_;
  bool ended_;
  PipelineStatus pipeline_status_;
  Demuxer::EncryptedMediaInitDataCB encrypted_media_init_data_cb_;
  VideoPixelFormat last_video_frame_format_;
  gfx::ColorSpace last_video_frame_color_space_;
  PipelineMetadata metadata_;
  scoped_refptr<VideoFrame> last_frame_;
  base::TimeDelta current_duration_;
  AudioRendererImpl::PlayDelayCBForTesting audio_play_delay_cb_;

  // By default RendererImpl will be created using CreateRendererImpl(). But
  // if |create_renderer_cb_| is set, it'll be used to create the Renderer
  // instead.
  using CreateRendererCB = base::RepeatingCallback<std::unique_ptr<Renderer>(
      std::optional<RendererType> renderer_type)>;
  CreateRendererCB create_renderer_cb_;

  std::unique_ptr<Renderer> CreateRendererImpl(
      std::optional<RendererType> renderer_type);

  // Sets |create_renderer_cb_| which will be used to wrap the Renderer created
  // by CreateRendererImpl().
  void SetCreateRendererCB(CreateRendererCB create_renderer_cb);

  PipelineStatus StartInternal(
      std::unique_ptr<DataSource> data_source,
      CdmContext* cdm_context,
      uint8_t test_type,
      CreateVideoDecodersCB prepend_video_decoders_cb = CreateVideoDecodersCB(),
      CreateAudioDecodersCB prepend_audio_decoders_cb =
          CreateAudioDecodersCB());

  PipelineStatus StartWithFile(
      const std::string& filename,
      CdmContext* cdm_context,
      uint8_t test_type,
      CreateVideoDecodersCB prepend_video_decoders_cb = CreateVideoDecodersCB(),
      CreateAudioDecodersCB prepend_audio_decoders_cb =
          CreateAudioDecodersCB());

  PipelineStatus StartPipelineWithMediaSource(TestMediaSource* source);
  PipelineStatus StartPipelineWithEncryptedMedia(
      TestMediaSource* source,
      FakeEncryptedMedia* encrypted_media);
  PipelineStatus StartPipelineWithMediaSource(
      TestMediaSource* source,
      uint8_t test_type,
      FakeEncryptedMedia* encrypted_media);
  PipelineStatus StartPipelineWithMediaSource(
      TestMediaSource* source,
      uint8_t test_type,
      CreateAudioDecodersCB prepend_audio_decoders_cb);

#if BUILDFLAG(ENABLE_HLS_DEMUXER)
  PipelineStatus StartPipelineWithHlsManifest(const std::string& filename);
#endif  // BUILDFLAG(ENABLE_HLS_DEMUXER)

  void OnSeeked(base::TimeDelta seek_time, PipelineStatus status);
  void OnStatusCallback(const base::RepeatingClosure& quit_run_loop_closure,
                        PipelineStatus status);
  void DemuxerEncryptedMediaInitDataCB(EmeInitDataType type,
                                       const std::vector<uint8_t>& init_data);

  void DemuxerMediaTracksUpdatedCB(std::unique_ptr<MediaTracks> tracks);

  void QuitAfterCurrentTimeTask(base::TimeDelta quit_time,
                                base::OnceClosure quit_closure);

  // Creates Demuxer and sets |demuxer_|.
  void CreateDemuxer(std::unique_ptr<DataSource> data_source);

  void OnVideoFramePaint(scoped_refptr<VideoFrame> frame);

  void CheckDuration();

  // Return the media start time from |demuxer_|.
  base::TimeDelta GetStartTime();

  MOCK_METHOD1(DecryptorAttached, void(bool));

  // Pipeline::Client overrides.
  void OnError(PipelineStatus status) override;
  void OnFallback(PipelineStatus status) override;
  void OnEnded() override;
  MOCK_METHOD1(OnMetadata, void(const PipelineMetadata&));
  MOCK_METHOD2(OnBufferingStateChange,
               void(BufferingState, BufferingStateChangeReason));
  MOCK_METHOD0(OnDurationChange, void());
  MOCK_METHOD1(OnWaiting, void(WaitingReason));
  MOCK_METHOD1(OnVideoNaturalSizeChange, void(const gfx::Size&));
  MOCK_METHOD1(OnVideoConfigChange, void(const VideoDecoderConfig&));
  MOCK_METHOD1(OnAudioConfigChange, void(const AudioDecoderConfig&));
  MOCK_METHOD1(OnVideoOpacityChange, void(bool));
  MOCK_METHOD1(OnVideoFrameRateChange, void(std::optional<int>));
  MOCK_METHOD0(OnVideoAverageKeyframeDistanceUpdate, void());
  MOCK_METHOD1(OnAudioPipelineInfoChange, void(const AudioPipelineInfo&));
  MOCK_METHOD1(OnVideoPipelineInfoChange, void(const VideoPipelineInfo&));
  MOCK_METHOD1(OnRemotePlayStateChange, void(MediaStatus::State state));

 private:
  // Runs |run_loop| until it is explicitly Quit() by some part of the calling
  // test fixture or when an error occurs (by setting |on_error_closure_|). The
  // |task_environment_| is RunUntilIdle() after the RunLoop finishes
  // running, before returning to the caller.
  void RunUntilQuitOrError(base::RunLoop* run_loop);

  // Configures |on_ended_closure_| to quit |run_loop| and then calls
  // RunUntilQuitOrError() on it.
  void RunUntilQuitOrEndedOrError(base::RunLoop* run_loop);

  // Implementation of `Pipeline::Client::OnBufferingStateChange()` used during
  // seeks.  This handles failed seeks as well as successful ones, which have
  // different behavior around exiting the seek.
  void OnBufferingStateChangeForSeek(BufferingState state,
                                     BufferingStateChangeReason reason);

#if BUILDFLAG(IS_WIN)
  // MediaFoundationAudioDecoder calls CoInitialize() when creating the decoder.
  base::win::ScopedCOMInitializer com_initializer_;
#endif  // BUILDFLAG(IS_WIN)

  CreateVideoDecodersCB prepend_video_decoders_cb_;
  CreateAudioDecodersCB prepend_audio_decoders_cb_;

  // First buffering state we get from the pipeline.
  std::optional<BufferingState> buffering_state_;

  base::OnceClosure on_ended_closure_;
  base::OnceClosure on_error_closure_;
};

}  // namespace media

#endif  // MEDIA_TEST_PIPELINE_INTEGRATION_TEST_BASE_H_
