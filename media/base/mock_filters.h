// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MOCK_FILTERS_H_
#define MEDIA_BASE_MOCK_FILTERS_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_encoder.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_config.h"
#include "media/base/cdm_context.h"
#include "media/base/cdm_factory.h"
#include "media/base/cdm_key_information.h"
#include "media/base/cdm_promise.h"
#include "media/base/cdm_promise_adapter.h"
#include "media/base/content_decryption_module.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer.h"
#include "media/base/media_client.h"
#include "media/base/media_track.h"
#include "media/base/pipeline.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer.h"
#include "media/base/renderer_client.h"
#include "media/base/renderer_factory.h"
#include "media/base/stream_parser.h"
#include "media/base/time_source.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_encoder.h"
#include "media/base/video_encoder_metrics_provider.h"
#include "media/base/video_frame.h"
#include "media/base/video_renderer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/origin.h"

namespace media {

class MockPipelineClient : public Pipeline::Client {
 public:
  MockPipelineClient();
  ~MockPipelineClient();

  MOCK_METHOD1(OnError, void(PipelineStatus));
  MOCK_METHOD1(OnFallback, void(PipelineStatus));
  MOCK_METHOD0(OnEnded, void());
  MOCK_METHOD1(OnMetadata, void(const PipelineMetadata&));
  MOCK_METHOD2(OnBufferingStateChange,
               void(BufferingState, BufferingStateChangeReason));
  MOCK_METHOD0(OnDurationChange, void());
  MOCK_METHOD1(OnWaiting, void(WaitingReason));
  MOCK_METHOD1(OnAudioConfigChange, void(const AudioDecoderConfig&));
  MOCK_METHOD1(OnVideoConfigChange, void(const VideoDecoderConfig&));
  MOCK_METHOD1(OnVideoNaturalSizeChange, void(const gfx::Size&));
  MOCK_METHOD1(OnVideoOpacityChange, void(bool));
  MOCK_METHOD1(OnVideoFrameRateChange, void(std::optional<int>));
  MOCK_METHOD0(OnVideoAverageKeyframeDistanceUpdate, void());
  MOCK_METHOD1(OnAudioPipelineInfoChange, void(const AudioPipelineInfo&));
  MOCK_METHOD1(OnVideoPipelineInfoChange, void(const VideoPipelineInfo&));
  MOCK_METHOD1(OnRemotePlayStateChange, void(MediaStatus::State state));
};

class MockPipeline : public Pipeline {
 public:
  MockPipeline();

  MockPipeline(const MockPipeline&) = delete;
  MockPipeline& operator=(const MockPipeline&) = delete;

  ~MockPipeline() override;

  void Start(StartType start_type,
             Demuxer* demuxer,
             Client* client,
             PipelineStatusCallback seek_cb) override {
    OnStart(start_type, demuxer, client, seek_cb);
  }
  MOCK_METHOD4(OnStart,
               void(StartType, Demuxer*, Client*, PipelineStatusCallback&));
  MOCK_METHOD0(Stop, void());
  void Seek(base::TimeDelta time, PipelineStatusCallback seek_cb) override {
    OnSeek(time, seek_cb);
  }
  MOCK_METHOD2(OnSeek, void(base::TimeDelta, PipelineStatusCallback&));
  void Suspend(PipelineStatusCallback cb) override { OnSuspend(cb); }
  MOCK_METHOD1(OnSuspend, void(PipelineStatusCallback&));
  void Resume(base::TimeDelta time, PipelineStatusCallback seek_cb) override {
    OnResume(time, seek_cb);
  }
  MOCK_METHOD2(OnResume, void(base::TimeDelta, PipelineStatusCallback&));
  MOCK_METHOD2(OnEnabledAudioTracksChanged,
               void(const std::vector<MediaTrack::Id>&, base::OnceClosure));
  MOCK_METHOD2(OnSelectedVideoTrackChanged,
               void(std::optional<MediaTrack::Id>, base::OnceClosure));
  MOCK_METHOD0(OnExternalVideoFrameRequest, void());

  // TODO(sandersd): This should automatically return true between Start() and
  // Stop(). (Or better, remove it from the interface entirely.)
  MOCK_CONST_METHOD0(IsRunning, bool());

  MOCK_CONST_METHOD0(IsSuspended, bool());

  // TODO(sandersd): These should be regular getters/setters.
  MOCK_CONST_METHOD0(GetPlaybackRate, double());
  MOCK_METHOD1(SetPlaybackRate, void(double));
  MOCK_CONST_METHOD0(GetVolume, float());
  MOCK_METHOD1(SetVolume, void(float));
  MOCK_METHOD1(SetLatencyHint, void(std::optional<base::TimeDelta>));
  MOCK_METHOD1(SetPreservesPitch, void(bool));
  MOCK_METHOD1(SetWasPlayedWithUserActivationAndHighMediaEngagement,
               void(bool));

  // TODO(sandersd): These should probably have setters too.
  MOCK_CONST_METHOD0(GetMediaTime, base::TimeDelta());
  MOCK_CONST_METHOD0(GetBufferedTimeRanges, Ranges<base::TimeDelta>());
  MOCK_CONST_METHOD0(GetMediaDuration, base::TimeDelta());
  MOCK_METHOD0(DidLoadingProgress, bool());
  MOCK_CONST_METHOD0(GetStatistics, PipelineStatistics());

  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) override {
    OnSetCdm(cdm_context, cdm_attached_cb);
  }
  MOCK_METHOD2(OnSetCdm,
               void(CdmContext* cdm_context, CdmAttachedCB& cdm_attached_cb));
};

class MockMediaResource : public MediaResource {
 public:
  MockMediaResource();
  ~MockMediaResource() override;

  // MediaResource implementation.
  MOCK_CONST_METHOD0(GetType, MediaResource::Type());
  MOCK_METHOD0(GetAllStreams, std::vector<DemuxerStream*>());
  MOCK_METHOD1(GetFirstStream, DemuxerStream*(DemuxerStream::Type type));
  MOCK_CONST_METHOD0(GetMediaUrlParams, const MediaUrlParams&());
};

class MockDemuxer : public Demuxer {
 public:
  MockDemuxer();

  MockDemuxer(const MockDemuxer&) = delete;
  MockDemuxer& operator=(const MockDemuxer&) = delete;

  ~MockDemuxer() override;

  // Demuxer implementation.
  std::string GetDisplayName() const override;
  DemuxerType GetDemuxerType() const override;

  void Initialize(DemuxerHost* host, PipelineStatusCallback cb) override {
    OnInitialize(host, cb);
  }
  MOCK_METHOD(void,
              OnInitialize,
              (DemuxerHost * host, PipelineStatusCallback& cb),
              ());
  MOCK_METHOD(void, StartWaitingForSeek, (base::TimeDelta), (override));
  MOCK_METHOD(void, CancelPendingSeek, (base::TimeDelta), (override));
  void Seek(base::TimeDelta time, PipelineStatusCallback cb) override {
    OnSeek(time, cb);
  }
  MOCK_METHOD(bool, IsSeekable, (), (const override));
  MOCK_METHOD(void,
              OnSeek,
              (base::TimeDelta time, PipelineStatusCallback& cb),
              ());
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void, AbortPendingReads, (), (override));
  MOCK_METHOD((std::vector<DemuxerStream*>), GetAllStreams, (), (override));

  MOCK_METHOD(base::TimeDelta, GetStartTime, (), (const, override));
  MOCK_METHOD(base::Time, GetTimelineOffset, (), (const, override));
  MOCK_METHOD(int64_t, GetMemoryUsage, (), (const, override));
  MOCK_METHOD(std::optional<container_names::MediaContainerName>,
              GetContainerForMetrics,
              (),
              (const, override));
  MOCK_METHOD(void,
              OnEnabledAudioTracksChanged,
              (const std::vector<MediaTrack::Id>&,
               base::TimeDelta,
               TrackChangeCB),
              (override));
  MOCK_METHOD(void,
              OnSelectedVideoTrackChanged,
              (const std::vector<MediaTrack::Id>&,
               base::TimeDelta,
               TrackChangeCB),
              (override));

  MOCK_METHOD(void, SetPlaybackRate, (double), (override));
};

class MockDemuxerStream : public DemuxerStream {
 public:
  explicit MockDemuxerStream(DemuxerStream::Type type);

  MockDemuxerStream(const MockDemuxerStream&) = delete;
  MockDemuxerStream& operator=(const MockDemuxerStream&) = delete;

  ~MockDemuxerStream() override;

  // DemuxerStream implementation.
  Type type() const override;
  StreamLiveness liveness() const override;
  void Read(uint32_t count, ReadCB read_cb) override { OnRead(read_cb); }
  MOCK_METHOD1(OnRead, void(ReadCB& read_cb));
  AudioDecoderConfig audio_decoder_config() override;
  VideoDecoderConfig video_decoder_config() override;
  MOCK_METHOD0(EnableBitstreamConverter, void());
  MOCK_METHOD0(SupportsConfigChanges, bool());

  void set_audio_decoder_config(const AudioDecoderConfig& config);
  void set_video_decoder_config(const VideoDecoderConfig& config);
  void set_liveness(StreamLiveness liveness);

 private:
  Type type_ = DemuxerStream::Type::UNKNOWN;
  StreamLiveness liveness_ = StreamLiveness::kUnknown;
  AudioDecoderConfig audio_decoder_config_;
  VideoDecoderConfig video_decoder_config_;
};

class MockVideoDecoder : public VideoDecoder {
 public:
  MockVideoDecoder();
  // Give a decoder a specific ID, like 42, so that different decoders in unit
  // tests can be differentiated from one another. All of these decoders have
  // a decoder type of kTesting, so that can't be used to differentiate them.
  explicit MockVideoDecoder(int decoder_id);
  MockVideoDecoder(bool is_platform_decoder,
                   bool supports_decryption,
                   int decoder_id);

  MockVideoDecoder(const MockVideoDecoder&) = delete;
  MockVideoDecoder& operator=(const MockVideoDecoder&) = delete;

  ~MockVideoDecoder() override;

  // Decoder implementation
  bool IsPlatformDecoder() const override;
  bool SupportsDecryption() const override;
  VideoDecoderType GetDecoderType() const override {
    return VideoDecoderType::kTesting;
  }

  // Allows getting the unique ID from a mock decoder so that they can be
  // identified during tests without having to add unique VideoDecoderTypes.
  int GetDecoderId() const { return decoder_id_; }

  // VideoDecoder implementation.
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override {
    Initialize_(config, low_delay, cdm_context, init_cb, output_cb, waiting_cb);
  }
  MOCK_METHOD6(Initialize_,
               void(const VideoDecoderConfig& config,
                    bool low_delay,
                    CdmContext* cdm_context,
                    InitCB& init_cb,
                    const OutputCB& output_cb,
                    const WaitingCB& waiting_cb));
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB cb) override {
    Decode_(std::move(buffer), cb);
  }
  MOCK_METHOD2(Decode_, void(scoped_refptr<DecoderBuffer> buffer, DecodeCB&));
  void Reset(base::OnceClosure cb) override { Reset_(cb); }
  MOCK_METHOD1(Reset_, void(base::OnceClosure&));
  MOCK_CONST_METHOD0(GetMaxDecodeRequests, int());
  MOCK_CONST_METHOD0(CanReadWithoutStalling, bool());
  MOCK_CONST_METHOD0(NeedsBitstreamConversion, bool());

 private:
  const bool is_platform_decoder_;
  const bool supports_decryption_;
  const int decoder_id_ = 0;
};

class MockAudioEncoder : public AudioEncoder {
 public:
  MockAudioEncoder();

  MockAudioEncoder(const MockAudioEncoder&) = delete;
  MockAudioEncoder& operator=(const MockAudioEncoder&) = delete;

  ~MockAudioEncoder() override;

  // AudioEncoder implementation.
  MOCK_METHOD(void,
              Initialize,
              (const Options& options,
               OutputCB output_cb,
               EncoderStatusCB done_cb),
              (override));

  MOCK_METHOD(void,
              Encode,
              (std::unique_ptr<AudioBus> audio_bus,
               base::TimeTicks capture_time,
               EncoderStatusCB done_cb),
              (override));

  MOCK_METHOD(void, Flush, (EncoderStatusCB done_cb), (override));
  MOCK_METHOD(void, DisablePostedCallbacks, (), (override));

  // A function for mocking destructor calls
  MOCK_METHOD(void, OnDestruct, ());
};

class MockVideoEncoder : public VideoEncoder {
 public:
  MockVideoEncoder();

  MockVideoEncoder(const MockVideoEncoder&) = delete;
  MockVideoEncoder& operator=(const MockVideoEncoder&) = delete;

  ~MockVideoEncoder() override;

  // VideoEncoder implementation.
  MOCK_METHOD(void,
              Initialize,
              (VideoCodecProfile profile,
               const Options& options,
               EncoderInfoCB info_cb,
               OutputCB output_cb,
               EncoderStatusCB done_cb),
              (override));

  MOCK_METHOD(void,
              Encode,
              (scoped_refptr<VideoFrame> frame,
               const EncodeOptions& encode_options,
               EncoderStatusCB done_cb),
              (override));

  MOCK_METHOD(void,
              ChangeOptions,
              (const Options& options,
               OutputCB output_cb,
               EncoderStatusCB done_cb),
              (override));

  MOCK_METHOD(void, Flush, (EncoderStatusCB done_cb), (override));
  MOCK_METHOD(void, DisablePostedCallbacks, (), (override));

  // A function for mocking destructor calls
  MOCK_METHOD(void, Dtor, ());
};

class MockAudioDecoder : public AudioDecoder {
 public:
  MockAudioDecoder();
  explicit MockAudioDecoder(int decoder_id);
  explicit MockAudioDecoder(bool is_platform_decoder,
                            bool supports_decryption,
                            int decoder_id);

  MockAudioDecoder(const MockAudioDecoder&) = delete;
  MockAudioDecoder& operator=(const MockAudioDecoder&) = delete;

  ~MockAudioDecoder() override;

  // Decoder implementation
  bool IsPlatformDecoder() const override;
  bool SupportsDecryption() const override;
  AudioDecoderType GetDecoderType() const override {
    return AudioDecoderType::kTesting;
  }

  // Allows getting the unique ID from a mock decoder so that they can be
  // identified during tests without having to add unique VideoDecoderTypes.
  int GetDecoderId() const { return decoder_id_; }

  // AudioDecoder implementation.
  void Initialize(const AudioDecoderConfig& config,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override {
    Initialize_(config, cdm_context, init_cb, output_cb, waiting_cb);
  }
  MOCK_METHOD5(Initialize_,
               void(const AudioDecoderConfig& config,
                    CdmContext* cdm_context,
                    InitCB& init_cb,
                    const OutputCB& output_cb,
                    const WaitingCB& waiting_cb));
  MOCK_METHOD2(Decode, void(scoped_refptr<DecoderBuffer> buffer, DecodeCB));
  void Reset(base::OnceClosure cb) override { Reset_(cb); }
  MOCK_METHOD1(Reset_, void(base::OnceClosure&));

 private:
  const bool is_platform_decoder_;
  const bool supports_decryption_;
  const int decoder_id_ = 0;
};

class MockRendererClient : public RendererClient {
 public:
  MockRendererClient();
  ~MockRendererClient();

  // RendererClient implementation.
  MOCK_METHOD1(OnError, void(PipelineStatus));
  MOCK_METHOD1(OnFallback, void(PipelineStatus));
  MOCK_METHOD0(OnEnded, void());
  MOCK_METHOD1(OnStatisticsUpdate, void(const PipelineStatistics&));
  MOCK_METHOD2(OnBufferingStateChange,
               void(BufferingState, BufferingStateChangeReason));
  MOCK_METHOD1(OnWaiting, void(WaitingReason));
  MOCK_METHOD1(OnAudioConfigChange, void(const AudioDecoderConfig&));
  MOCK_METHOD1(OnVideoConfigChange, void(const VideoDecoderConfig&));
  MOCK_METHOD1(OnVideoNaturalSizeChange, void(const gfx::Size&));
  MOCK_METHOD1(OnVideoOpacityChange, void(bool));
  MOCK_METHOD1(OnVideoFrameRateChange, void(std::optional<int>));
  MOCK_METHOD1(OnDurationChange, void(base::TimeDelta));
  MOCK_METHOD1(OnRemotePlayStateChange, void(MediaStatus::State state));
  MOCK_METHOD0(IsVideoStreamAvailable, bool());
};

class MockVideoRenderer : public VideoRenderer {
 public:
  MockVideoRenderer();

  MockVideoRenderer(const MockVideoRenderer&) = delete;
  MockVideoRenderer& operator=(const MockVideoRenderer&) = delete;

  ~MockVideoRenderer() override;

  // VideoRenderer implementation.
  void Initialize(DemuxerStream* stream,
                  CdmContext* cdm_context,
                  RendererClient* client,
                  const TimeSource::WallClockTimeCB& wall_clock_time_cb,
                  PipelineStatusCallback init_cb) override {
    OnInitialize(stream, cdm_context, client, wall_clock_time_cb, init_cb);
  }
  MOCK_METHOD5(OnInitialize,
               void(DemuxerStream* stream,
                    CdmContext* cdm_context,
                    RendererClient* client,
                    const TimeSource::WallClockTimeCB& wall_clock_time_cb,
                    PipelineStatusCallback& init_cb));
  MOCK_METHOD1(Flush, void(base::OnceClosure flush_cb));
  MOCK_METHOD1(StartPlayingFrom, void(base::TimeDelta));
  MOCK_METHOD0(OnTimeProgressing, void());
  MOCK_METHOD0(OnTimeStopped, void());
  MOCK_METHOD1(SetLatencyHint,
               void(std::optional<base::TimeDelta> latency_hint));
};

class MockAudioRenderer : public AudioRenderer {
 public:
  MockAudioRenderer();

  MockAudioRenderer(const MockAudioRenderer&) = delete;
  MockAudioRenderer& operator=(const MockAudioRenderer&) = delete;

  ~MockAudioRenderer() override;

  // AudioRenderer implementation.
  void Initialize(DemuxerStream* stream,
                  CdmContext* cdm_context,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) override {
    OnInitialize(stream, cdm_context, client, init_cb);
  }
  MOCK_METHOD4(OnInitialize,
               void(DemuxerStream* stream,
                    CdmContext* cdm_context,
                    RendererClient* client,
                    PipelineStatusCallback& init_cb));
  MOCK_METHOD0(GetTimeSource, TimeSource*());
  MOCK_METHOD1(Flush, void(base::OnceClosure flush_cb));
  MOCK_METHOD0(StartPlaying, void());
  MOCK_METHOD1(SetVolume, void(float volume));
  MOCK_METHOD1(SetLatencyHint,
               void(std::optional<base::TimeDelta> latency_hint));
  MOCK_METHOD1(SetPreservesPitch, void(bool));
  MOCK_METHOD1(SetWasPlayedWithUserActivationAndHighMediaEngagement,
               void(bool));
};

class MockRenderer : public Renderer {
 public:
  MockRenderer();

  MockRenderer(const MockRenderer&) = delete;
  MockRenderer& operator=(const MockRenderer&) = delete;

  ~MockRenderer() override;

  // Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) override {
    OnInitialize(media_resource, client, init_cb);
  }
  MOCK_METHOD3(OnInitialize,
               void(MediaResource* media_resource,
                    RendererClient* client,
                    PipelineStatusCallback& init_cb));
  MOCK_METHOD1(SetLatencyHint, void(std::optional<base::TimeDelta>));
  MOCK_METHOD1(SetPreservesPitch, void(bool));
  MOCK_METHOD1(SetWasPlayedWithUserActivationAndHighMediaEngagement,
               void(bool));
  void Flush(base::OnceClosure flush_cb) override { OnFlush(flush_cb); }
  MOCK_METHOD1(OnFlush, void(base::OnceClosure& flush_cb));
  MOCK_METHOD1(StartPlayingFrom, void(base::TimeDelta timestamp));
  MOCK_METHOD1(SetPlaybackRate, void(double playback_rate));
  MOCK_METHOD1(SetVolume, void(float volume));
  MOCK_METHOD0(GetMediaTime, base::TimeDelta());
  MOCK_METHOD0(HasAudio, bool());
  MOCK_METHOD0(HasVideo, bool());
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) override {
    OnSetCdm(cdm_context, cdm_attached_cb);
  }
  MOCK_METHOD2(OnSetCdm,
               void(CdmContext* cdm_context, CdmAttachedCB& cdm_attached_cb));
  MOCK_METHOD2(OnSelectedVideoTrackChanged,
               void(std::vector<DemuxerStream*>, base::OnceClosure));
  MOCK_METHOD2(OnSelectedAudioTracksChanged,
               void(std::vector<DemuxerStream*>, base::OnceClosure));
  RendererType GetRendererType() override { return RendererType::kTest; }

  base::WeakPtr<MockRenderer> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockRenderer> weak_ptr_factory_{this};
};

class MockRendererFactory : public RendererFactory {
 public:
  MockRendererFactory();

  MockRendererFactory(const MockRendererFactory&) = delete;
  MockRendererFactory& operator=(const MockRendererFactory&) = delete;

  ~MockRendererFactory() override;

  // Renderer implementation.
  MOCK_METHOD6(
      CreateRenderer,
      std::unique_ptr<Renderer>(const scoped_refptr<base::SequencedTaskRunner>&,
                                const scoped_refptr<base::TaskRunner>&,
                                AudioRendererSink*,
                                VideoRendererSink*,
                                RequestOverlayInfoCB,
                                const gfx::ColorSpace&));
};

class MockTimeSource : public TimeSource {
 public:
  MockTimeSource();

  MockTimeSource(const MockTimeSource&) = delete;
  MockTimeSource& operator=(const MockTimeSource&) = delete;

  ~MockTimeSource() override;

  // TimeSource implementation.
  MOCK_METHOD0(StartTicking, void());
  MOCK_METHOD0(StopTicking, void());
  MOCK_METHOD1(SetPlaybackRate, void(double));
  MOCK_METHOD1(SetMediaTime, void(base::TimeDelta));
  MOCK_METHOD0(CurrentMediaTime, base::TimeDelta());
  MOCK_METHOD2(GetWallClockTimes,
               bool(const std::vector<base::TimeDelta>&,
                    std::vector<base::TimeTicks>*));
};

// Mock CDM callbacks.
// TODO(xhwang): This could be a subclass of CdmClient if we plan to add one.
// See http://crbug.com/657940
class MockCdmClient {
 public:
  MockCdmClient();
  virtual ~MockCdmClient();

  MOCK_METHOD3(OnSessionMessage,
               void(const std::string& session_id,
                    CdmMessageType message_type,
                    const std::vector<uint8_t>& message));
  MOCK_METHOD2(OnSessionClosed,
               void(const std::string& session_id,
                    CdmSessionClosedReason reason));

  // Add OnSessionKeysChangeCalled() function so we can store |keys_info|.
  MOCK_METHOD2(OnSessionKeysChangeCalled,
               void(const std::string& session_id,
                    bool has_additional_usable_key));
  void OnSessionKeysChange(const std::string& session_id,
                           bool has_additional_usable_key,
                           CdmKeysInfo keys_info) {
    keys_info_.swap(keys_info);
    OnSessionKeysChangeCalled(session_id, has_additional_usable_key);
  }

  MOCK_METHOD2(OnSessionExpirationUpdate,
               void(const std::string& session_id, base::Time new_expiry_time));

  const CdmKeysInfo& keys_info() const { return keys_info_; }

 private:
  CdmKeysInfo keys_info_;
};

class MockDecryptor : public Decryptor {
 public:
  MockDecryptor();

  MockDecryptor(const MockDecryptor&) = delete;
  MockDecryptor& operator=(const MockDecryptor&) = delete;

  ~MockDecryptor() override;

  MOCK_METHOD3(Decrypt,
               void(StreamType stream_type,
                    scoped_refptr<DecoderBuffer> encrypted,
                    DecryptCB decrypt_cb));
  MOCK_METHOD1(CancelDecrypt, void(StreamType stream_type));
  MOCK_METHOD2(InitializeAudioDecoder,
               void(const AudioDecoderConfig& config, DecoderInitCB init_cb));
  MOCK_METHOD2(InitializeVideoDecoder,
               void(const VideoDecoderConfig& config, DecoderInitCB init_cb));
  MOCK_METHOD2(DecryptAndDecodeAudio,
               void(scoped_refptr<DecoderBuffer> encrypted,
                    AudioDecodeCB audio_decode_cb));
  MOCK_METHOD2(DecryptAndDecodeVideo,
               void(scoped_refptr<DecoderBuffer> encrypted,
                    VideoDecodeCB video_decode_cb));
  MOCK_METHOD1(ResetDecoder, void(StreamType stream_type));
  MOCK_METHOD1(DeinitializeDecoder, void(StreamType stream_type));
  MOCK_METHOD0(CanAlwaysDecrypt, bool());
};

class MockCdmContext : public CdmContext {
 public:
  MockCdmContext();

  MockCdmContext(const MockCdmContext&) = delete;
  MockCdmContext& operator=(const MockCdmContext&) = delete;

  ~MockCdmContext() override;

  MOCK_METHOD1(RegisterEventCB,
               std::unique_ptr<CallbackRegistration>(EventCB event_cb));
  MOCK_METHOD0(GetDecryptor, Decryptor*());

#if BUILDFLAG(IS_WIN)
  MOCK_METHOD0(RequiresMediaFoundationRenderer, bool());
  MOCK_METHOD0(GetMediaFoundationCdmProxy,
               scoped_refptr<MediaFoundationCdmProxy>());
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  MOCK_METHOD0(GetChromeOsCdmContext, chromeos::ChromeOsCdmContext*());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  std::optional<base::UnguessableToken> GetCdmId() const override;

  void set_cdm_id(const base::UnguessableToken& cdm_id);

 private:
  std::optional<base::UnguessableToken> cdm_id_;
};

class MockCdmPromise : public SimpleCdmPromise {
 public:
  // |expect_success| is true if resolve() should be called, false if reject()
  // is expected.
  explicit MockCdmPromise(bool expect_success);

  MockCdmPromise(const MockCdmPromise&) = delete;
  MockCdmPromise& operator=(const MockCdmPromise&) = delete;

  ~MockCdmPromise() override;

  MOCK_METHOD0(resolve, void());
  MOCK_METHOD3(reject,
               void(CdmPromise::Exception, uint32_t, const std::string&));
};

class MockCdmSessionPromise : public NewSessionCdmPromise {
 public:
  // |expect_success| is true if resolve() should be called, false if reject()
  // is expected. |new_session_id| is updated with the new session's ID on
  // resolve().
  MockCdmSessionPromise(bool expect_success, std::string* new_session_id);

  MockCdmSessionPromise(const MockCdmSessionPromise&) = delete;
  MockCdmSessionPromise& operator=(const MockCdmSessionPromise&) = delete;

  ~MockCdmSessionPromise() override;

  MOCK_METHOD1(resolve, void(const std::string&));
  MOCK_METHOD3(reject,
               void(CdmPromise::Exception, uint32_t, const std::string&));
};

class MockCdmKeyStatusPromise : public KeyStatusCdmPromise {
 public:
  // |expect_success| is true if resolve() should be called, false if reject()
  // is expected. |key_status| is updated with the key status on resolve(),
  // |exception| is updated with the exception on reject() if |exception| is
  // set.
  MockCdmKeyStatusPromise(bool expect_success,
                          CdmKeyInformation::KeyStatus* key_status,
                          CdmPromise::Exception* exception = nullptr);

  MockCdmKeyStatusPromise(const MockCdmKeyStatusPromise&) = delete;
  MockCdmKeyStatusPromise& operator=(const MockCdmKeyStatusPromise&) = delete;

  ~MockCdmKeyStatusPromise() override;

  MOCK_METHOD1(resolve, void(const CdmKeyInformation::KeyStatus&));
  MOCK_METHOD3(reject,
               void(CdmPromise::Exception, uint32_t, const std::string&));
};

class MockCdm : public ContentDecryptionModule {
 public:
  MockCdm();
  MockCdm(const CdmConfig& cdm_config,
          const SessionMessageCB& session_message_cb,
          const SessionClosedCB& session_closed_cb,
          const SessionKeysChangeCB& session_keys_change_cb,
          const SessionExpirationUpdateCB& session_expiration_update_cb);

  MockCdm(const MockCdm&) = delete;
  MockCdm& operator=(const MockCdm&) = delete;

  void Initialize(
      const CdmConfig& cdm_config,
      const SessionMessageCB& session_message_cb,
      const SessionClosedCB& session_closed_cb,
      const SessionKeysChangeCB& session_keys_change_cb,
      const SessionExpirationUpdateCB& session_expiration_update_cb);

  // ContentDecryptionModule implementation.
  MOCK_METHOD2(SetServerCertificate,
               void(const std::vector<uint8_t>& certificate,
                    std::unique_ptr<SimpleCdmPromise> promise));
  MOCK_METHOD4(CreateSessionAndGenerateRequest,
               void(CdmSessionType session_type,
                    EmeInitDataType init_data_type,
                    const std::vector<uint8_t>& init_data,
                    std::unique_ptr<NewSessionCdmPromise> promise));
  MOCK_METHOD3(LoadSession,
               void(CdmSessionType session_type,
                    const std::string& session_id,
                    std::unique_ptr<NewSessionCdmPromise> promise));
  MOCK_METHOD3(UpdateSession,
               void(const std::string& session_id,
                    const std::vector<uint8_t>& response,
                    std::unique_ptr<SimpleCdmPromise> promise));
  MOCK_METHOD2(CloseSession,
               void(const std::string& session_id,
                    std::unique_ptr<SimpleCdmPromise> promise));
  MOCK_METHOD2(RemoveSession,
               void(const std::string& session_id,
                    std::unique_ptr<SimpleCdmPromise> promise));

  MOCK_METHOD0(GetCdmContext, CdmContext*());

  void CallSessionMessageCB(const std::string& session_id,
                            CdmMessageType message_type,
                            const std::vector<uint8_t>& message);
  void CallSessionClosedCB(const std::string& session_id,
                           CdmSessionClosedReason reason);
  void CallSessionKeysChangeCB(const std::string& session_id,
                               bool has_additional_usable_key,
                               CdmKeysInfo keys_info);
  void CallSessionExpirationUpdateCB(const std::string& session_id,
                                     base::Time new_expiry_time);

  const std::string& GetKeySystem() const { return key_system_; }

 protected:
  ~MockCdm() override;

 private:
  std::string key_system_;
  SessionMessageCB session_message_cb_;
  SessionClosedCB session_closed_cb_;
  SessionKeysChangeCB session_keys_change_cb_;
  SessionExpirationUpdateCB session_expiration_update_cb_;
};

class MockCdmFactory : public CdmFactory {
 public:
  explicit MockCdmFactory(scoped_refptr<MockCdm> cdm);

  MockCdmFactory(const MockCdmFactory&) = delete;
  MockCdmFactory& operator=(const MockCdmFactory&) = delete;

  ~MockCdmFactory() override;

  // CdmFactory implementation.
  // This creates a StrictMock<MockCdm> when called. Although ownership of the
  // created CDM is passed to |cdm_created_cb|, a copy is kept (and available
  // using Cdm()). If |key_system| is empty, no CDM will be created.
  void Create(const CdmConfig& cdm_config,
              const SessionMessageCB& session_message_cb,
              const SessionClosedCB& session_closed_cb,
              const SessionKeysChangeCB& session_keys_change_cb,
              const SessionExpirationUpdateCB& session_expiration_update_cb,
              CdmCreatedCB cdm_created_cb) override;

  // Provide a callback to be called before the CDM is created and returned.
  void SetBeforeCreationCB(base::RepeatingClosure before_creation_cb);

 private:
  // Reference to the created CDM.
  scoped_refptr<MockCdm> mock_cdm_;

  // Callback to be used before Create() successfully calls |cdm_created_cb|.
  base::RepeatingClosure before_creation_cb_;
};

class MockStreamParser : public StreamParser {
 public:
  MockStreamParser();

  MockStreamParser(const MockStreamParser&) = delete;
  MockStreamParser& operator=(const MockStreamParser&) = delete;

  ~MockStreamParser() override;

  // StreamParser interface
  MOCK_METHOD7(Init,
               void(InitCB init_cb,
                    NewConfigCB config_cb,
                    NewBuffersCB new_buffers_cb,
                    EncryptedMediaInitDataCB encrypted_media_init_data_cb,
                    NewMediaSegmentCB new_segment_cb,
                    EndMediaSegmentCB end_of_segment_cb,
                    MediaLog* media_log));
  MOCK_METHOD0(Flush, void());
  MOCK_CONST_METHOD0(GetGenerateTimestampsFlag, bool());
  MOCK_METHOD1(AppendToParseBuffer, bool(base::span<const uint8_t>));
  MOCK_METHOD1(Parse, ParseStatus(int));
};

class MockMediaClient : public media::MediaClient {
 public:
  MockMediaClient();

  MockMediaClient(const MockMediaClient&) = delete;
  MockMediaClient& operator=(const MockMediaClient&) = delete;

  ~MockMediaClient() override;

  // MediaClient implementation.
  MOCK_METHOD1(IsSupportedAudioType, bool(const media::AudioType& type));
  MOCK_METHOD1(IsSupportedVideoType, bool(const media::VideoType& type));
  MOCK_METHOD1(IsSupportedBitstreamAudioCodec, bool(media::AudioCodec codec));
  MOCK_METHOD1(GetAudioRendererAlgorithmParameters,
               std::optional<::media::AudioRendererAlgorithmParameters>(
                   media::AudioParameters audio_parameters));
  MOCK_METHOD(media::ExternalMemoryAllocator*,
              GetMediaAllocator,
              (),
              (override));
};

class MockVideoEncoderMetricsProvider : public VideoEncoderMetricsProvider {
 public:
  MockVideoEncoderMetricsProvider();
  ~MockVideoEncoderMetricsProvider() override;

  void Initialize(VideoCodecProfile codec_profile,
                  const gfx::Size& encode_size,
                  bool is_hardware_encoder,
                  SVCScalabilityMode svc_mode) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    MockInitialize(codec_profile, encode_size, is_hardware_encoder, svc_mode);
  }
  void IncrementEncodedFrameCount() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    MockIncrementEncodedFrameCount();
  }
  void SetError(const media::EncoderStatus& status) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    MockSetError(status);
  }

  MOCK_METHOD(
      void,
      MockInitialize,
      (VideoCodecProfile, (const gfx::Size&), bool, SVCScalabilityMode));
  MOCK_METHOD(void, MockIncrementEncodedFrameCount, ());
  MOCK_METHOD(void, MockSetError, (const media::EncoderStatus&));
  MOCK_METHOD(void, MockDestroy, ());

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};
}  // namespace media

#endif  // MEDIA_BASE_MOCK_FILTERS_H_
