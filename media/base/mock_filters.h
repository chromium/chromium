// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MOCK_FILTERS_H_
#define MEDIA_BASE_MOCK_FILTERS_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer.h"
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
#include "media/base/text_track.h"
#include "media/base/text_track_config.h"
#include "media/base/time_source.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
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
  MOCK_METHOD0(OnEnded, void());
  MOCK_METHOD1(OnMetadata, void(const PipelineMetadata&));
  MOCK_METHOD2(OnBufferingStateChange,
               void(BufferingState, BufferingStateChangeReason));
  MOCK_METHOD0(OnDurationChange, void());
  MOCK_METHOD2(OnAddTextTrack,
               void(const TextTrackConfig&, const AddTextTrackDoneCB&));
  MOCK_METHOD1(OnWaiting, void(WaitingReason));
  MOCK_METHOD1(OnAudioConfigChange, void(const AudioDecoderConfig&));
  MOCK_METHOD1(OnVideoConfigChange, void(const VideoDecoderConfig&));
  MOCK_METHOD1(OnVideoNaturalSizeChange, void(const gfx::Size&));
  MOCK_METHOD1(OnVideoOpacityChange, void(bool));
  MOCK_METHOD0(OnVideoAverageKeyframeDistanceUpdate, void());
  MOCK_METHOD1(OnAudioDecoderChange, void(const PipelineDecoderInfo&));
  MOCK_METHOD1(OnVideoDecoderChange, void(const PipelineDecoderInfo&));
  MOCK_METHOD1(OnRemotePlayStateChange, void(MediaStatus::State state));
};

class MockPipeline : public Pipeline {
 public:
  MockPipeline();
  ~MockPipeline() override;

  MOCK_METHOD4(Start,
               void(StartType, Demuxer*, Client*, const PipelineStatusCB&));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD2(Seek, void(base::TimeDelta, const PipelineStatusCB&));
  MOCK_METHOD1(Suspend, void(const PipelineStatusCB&));
  MOCK_METHOD2(Resume, void(base::TimeDelta, const PipelineStatusCB&));
  MOCK_METHOD2(OnEnabledAudioTracksChanged,
               void(const std::vector<MediaTrack::Id>&, base::OnceClosure));
  MOCK_METHOD2(OnSelectedVideoTrackChanged,
               void(base::Optional<MediaTrack::Id>, base::OnceClosure));

  // TODO(sandersd): This should automatically return true between Start() and
  // Stop(). (Or better, remove it from the interface entirely.)
  MOCK_CONST_METHOD0(IsRunning, bool());

  MOCK_CONST_METHOD0(IsSuspended, bool());

  // TODO(sandersd): These should be regular getters/setters.
  MOCK_CONST_METHOD0(GetPlaybackRate, double());
  MOCK_METHOD1(SetPlaybackRate, void(double));
  MOCK_CONST_METHOD0(GetVolume, float());
  MOCK_METHOD1(SetVolume, void(float));

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

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPipeline);
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
  ~MockDemuxer() override;

  // Demuxer implementation.
  std::string GetDisplayName() const override;
  void Initialize(DemuxerHost* host, PipelineStatusCallback cb) {
    OnInitialize(host, cb);
  }
  MOCK_METHOD2(OnInitialize,
               void(DemuxerHost* host, PipelineStatusCallback& cb));
  MOCK_METHOD1(StartWaitingForSeek, void(base::TimeDelta));
  MOCK_METHOD1(CancelPendingSeek, void(base::TimeDelta));
  void Seek(base::TimeDelta time, PipelineStatusCallback cb) {
    OnSeek(time, cb);
  }
  MOCK_METHOD2(OnSeek, void(base::TimeDelta time, PipelineStatusCallback& cb));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(AbortPendingReads, void());
  MOCK_METHOD0(GetAllStreams, std::vector<DemuxerStream*>());

  MOCK_CONST_METHOD0(GetStartTime, base::TimeDelta());
  MOCK_CONST_METHOD0(GetTimelineOffset, base::Time());
  MOCK_CONST_METHOD0(GetMemoryUsage, int64_t());
  MOCK_METHOD3(OnEnabledAudioTracksChanged,
               void(const std::vector<MediaTrack::Id>&,
                    base::TimeDelta,
                    TrackChangeCB));
  MOCK_METHOD3(OnSelectedVideoTrackChanged,
               void(const std::vector<MediaTrack::Id>&,
                    base::TimeDelta,
                    TrackChangeCB));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDemuxer);
};

class MockDemuxerStream : public DemuxerStream {
 public:
  explicit MockDemuxerStream(DemuxerStream::Type type);
  ~MockDemuxerStream() override;

  // DemuxerStream implementation.
  Type type() const override;
  Liveness liveness() const override;
  void Read(ReadCB read_cb) { OnRead(read_cb); }
  MOCK_METHOD1(OnRead, void(ReadCB& read_cb));
  MOCK_CONST_METHOD0(IsReadPending, bool());
  AudioDecoderConfig audio_decoder_config() override;
  VideoDecoderConfig video_decoder_config() override;
  MOCK_METHOD0(EnableBitstreamConverter, void());
  MOCK_METHOD0(SupportsConfigChanges, bool());

  void set_audio_decoder_config(const AudioDecoderConfig& config);
  void set_video_decoder_config(const VideoDecoderConfig& config);
  void set_liveness(Liveness liveness);

 private:
  Type type_;
  Liveness liveness_;
  AudioDecoderConfig audio_decoder_config_;
  VideoDecoderConfig video_decoder_config_;

  DISALLOW_COPY_AND_ASSIGN(MockDemuxerStream);
};

class MockVideoDecoder : public VideoDecoder {
 public:
  explicit MockVideoDecoder(
      const std::string& decoder_name = "MockVideoDecoder");
  ~MockVideoDecoder() override;

  // VideoDecoder implementation.
  std::string GetDisplayName() const override;
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
  std::string decoder_name_;
  DISALLOW_COPY_AND_ASSIGN(MockVideoDecoder);
};

class MockAudioDecoder : public AudioDecoder {
 public:
  explicit MockAudioDecoder(
      const std::string& decoder_name = "MockAudioDecoder");
  ~MockAudioDecoder() override;

  // AudioDecoder implementation.
  std::string GetDisplayName() const override;
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
  MOCK_METHOD2(Decode,
               void(scoped_refptr<DecoderBuffer> buffer, const DecodeCB&));
  void Reset(base::OnceClosure cb) override { Reset_(cb); }
  MOCK_METHOD1(Reset_, void(base::OnceClosure&));

 private:
  std::string decoder_name_;
  DISALLOW_COPY_AND_ASSIGN(MockAudioDecoder);
};

class MockRendererClient : public RendererClient {
 public:
  MockRendererClient();
  ~MockRendererClient();

  // RendererClient implementation.
  MOCK_METHOD1(OnError, void(PipelineStatus));
  MOCK_METHOD0(OnEnded, void());
  MOCK_METHOD1(OnStatisticsUpdate, void(const PipelineStatistics&));
  MOCK_METHOD2(OnBufferingStateChange,
               void(BufferingState, BufferingStateChangeReason));
  MOCK_METHOD1(OnWaiting, void(WaitingReason));
  MOCK_METHOD1(OnAudioConfigChange, void(const AudioDecoderConfig&));
  MOCK_METHOD1(OnVideoConfigChange, void(const VideoDecoderConfig&));
  MOCK_METHOD1(OnVideoNaturalSizeChange, void(const gfx::Size&));
  MOCK_METHOD1(OnVideoOpacityChange, void(bool));
  MOCK_METHOD1(OnDurationChange, void(base::TimeDelta));
  MOCK_METHOD1(OnRemotePlayStateChange, void(MediaStatus::State state));
  MOCK_METHOD0(IsVideoStreamAvailable, bool());
};

class MockVideoRenderer : public VideoRenderer {
 public:
  MockVideoRenderer();
  ~MockVideoRenderer() override;

  // VideoRenderer implementation.
  MOCK_METHOD5(Initialize,
               void(DemuxerStream* stream,
                    CdmContext* cdm_context,
                    RendererClient* client,
                    const TimeSource::WallClockTimeCB& wall_clock_time_cb,
                    const PipelineStatusCB& init_cb));
  MOCK_METHOD1(Flush, void(const base::Closure& callback));
  MOCK_METHOD1(StartPlayingFrom, void(base::TimeDelta));
  MOCK_METHOD0(OnTimeProgressing, void());
  MOCK_METHOD0(OnTimeStopped, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoRenderer);
};

class MockAudioRenderer : public AudioRenderer {
 public:
  MockAudioRenderer();
  ~MockAudioRenderer() override;

  // AudioRenderer implementation.
  MOCK_METHOD4(Initialize,
               void(DemuxerStream* stream,
                    CdmContext* cdm_context,
                    RendererClient* client,
                    const PipelineStatusCB& init_cb));
  MOCK_METHOD0(GetTimeSource, TimeSource*());
  MOCK_METHOD1(Flush, void(const base::Closure& callback));
  MOCK_METHOD0(StartPlaying, void());
  MOCK_METHOD1(SetVolume, void(float volume));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioRenderer);
};

class MockRenderer : public Renderer {
 public:
  MockRenderer();
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
  void Flush(base::OnceClosure flush_cb) { OnFlush(flush_cb); }
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

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRenderer);
};

class MockRendererFactory : public RendererFactory {
 public:
  MockRendererFactory();
  ~MockRendererFactory() override;

  // Renderer implementation.
  MOCK_METHOD6(CreateRenderer,
               std::unique_ptr<Renderer>(
                   const scoped_refptr<base::SingleThreadTaskRunner>&,
                   const scoped_refptr<base::TaskRunner>&,
                   AudioRendererSink*,
                   VideoRendererSink*,
                   const RequestOverlayInfoCB&,
                   const gfx::ColorSpace&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRendererFactory);
};

class MockTimeSource : public TimeSource {
 public:
  MockTimeSource();
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

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTimeSource);
};

class MockTextTrack : public TextTrack {
 public:
  MockTextTrack();
  ~MockTextTrack() override;

  MOCK_METHOD5(addWebVTTCue,
               void(base::TimeDelta start,
                    base::TimeDelta end,
                    const std::string& id,
                    const std::string& content,
                    const std::string& settings));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTextTrack);
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
  MOCK_METHOD1(OnSessionClosed, void(const std::string& session_id));

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
  ~MockDecryptor() override;

  MOCK_METHOD2(RegisterNewKeyCB,
               void(StreamType stream_type, const NewKeyCB& new_key_cb));
  MOCK_METHOD3(Decrypt,
               void(StreamType stream_type,
                    scoped_refptr<DecoderBuffer> encrypted,
                    const DecryptCB& decrypt_cb));
  MOCK_METHOD1(CancelDecrypt, void(StreamType stream_type));
  MOCK_METHOD2(InitializeAudioDecoder,
               void(const AudioDecoderConfig& config,
                    const DecoderInitCB& init_cb));
  MOCK_METHOD2(InitializeVideoDecoder,
               void(const VideoDecoderConfig& config,
                    const DecoderInitCB& init_cb));
  MOCK_METHOD2(DecryptAndDecodeAudio,
               void(scoped_refptr<DecoderBuffer> encrypted,
                    const AudioDecodeCB& audio_decode_cb));
  MOCK_METHOD2(DecryptAndDecodeVideo,
               void(scoped_refptr<DecoderBuffer> encrypted,
                    const VideoDecodeCB& video_decode_cb));
  MOCK_METHOD1(ResetDecoder, void(StreamType stream_type));
  MOCK_METHOD1(DeinitializeDecoder, void(StreamType stream_type));
  MOCK_METHOD0(CanAlwaysDecrypt, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDecryptor);
};

class MockCdmContext : public CdmContext {
 public:
  MockCdmContext();
  ~MockCdmContext() override;

  MOCK_METHOD0(GetDecryptor, Decryptor*());
  int GetCdmId() const override;

  void set_cdm_id(int cdm_id);

 private:
  int cdm_id_ = CdmContext::kInvalidCdmId;

  DISALLOW_COPY_AND_ASSIGN(MockCdmContext);
};

class MockCdmPromise : public SimpleCdmPromise {
 public:
  // |expect_success| is true if resolve() should be called, false if reject()
  // is expected.
  explicit MockCdmPromise(bool expect_success);
  ~MockCdmPromise() override;

  MOCK_METHOD0(resolve, void());
  MOCK_METHOD3(reject,
               void(CdmPromise::Exception, uint32_t, const std::string&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCdmPromise);
};

class MockCdmSessionPromise : public NewSessionCdmPromise {
 public:
  // |expect_success| is true if resolve() should be called, false if reject()
  // is expected. |new_session_id| is updated with the new session's ID on
  // resolve().
  MockCdmSessionPromise(bool expect_success, std::string* new_session_id);
  ~MockCdmSessionPromise() override;

  MOCK_METHOD1(resolve, void(const std::string&));
  MOCK_METHOD3(reject,
               void(CdmPromise::Exception, uint32_t, const std::string&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCdmSessionPromise);
};

class MockCdm : public ContentDecryptionModule {
 public:
  MockCdm(const std::string& key_system,
          const url::Origin& security_origin,
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
  void CallSessionClosedCB(const std::string& session_id);
  void CallSessionKeysChangeCB(const std::string& session_id,
                               bool has_additional_usable_key,
                               CdmKeysInfo keys_info);
  void CallSessionExpirationUpdateCB(const std::string& session_id,
                                     base::Time new_expiry_time);

  const std::string& GetKeySystem() const { return key_system_; }
  const url::Origin& GetSecurityOrigin() const { return security_origin_; }

 protected:
  ~MockCdm() override;

 private:
  std::string key_system_;
  url::Origin security_origin_;

  // Callbacks.
  SessionMessageCB session_message_cb_;
  SessionClosedCB session_closed_cb_;
  SessionKeysChangeCB session_keys_change_cb_;
  SessionExpirationUpdateCB session_expiration_update_cb_;

  DISALLOW_COPY_AND_ASSIGN(MockCdm);
};

class MockCdmFactory : public CdmFactory {
 public:
  MockCdmFactory();
  ~MockCdmFactory() override;

  // CdmFactory implementation.
  // This creates a StrictMock<MockCdm> when called. Although ownership of the
  // created CDM is passed to |cdm_created_cb|, a copy is kept (and available
  // using Cdm()). If |key_system| is empty, no CDM will be created.
  void Create(const std::string& key_system,
              const url::Origin& security_origin,
              const CdmConfig& cdm_config,
              const SessionMessageCB& session_message_cb,
              const SessionClosedCB& session_closed_cb,
              const SessionKeysChangeCB& session_keys_change_cb,
              const SessionExpirationUpdateCB& session_expiration_update_cb,
              const CdmCreatedCB& cdm_created_cb) override;

  // Return a pointer to the created CDM.
  MockCdm* GetCreatedCdm();

  // Provide a callback to be called before the CDM is created and returned.
  void SetBeforeCreationCB(const base::Closure& before_creation_cb);

 private:
  // Reference to the created CDM.
  scoped_refptr<MockCdm> created_cdm_;

  // Callback to be used before Create() successfully calls |cdm_created_cb|.
  base::Closure before_creation_cb_;

  DISALLOW_COPY_AND_ASSIGN(MockCdmFactory);
};

class MockStreamParser : public StreamParser {
 public:
  MockStreamParser();
  ~MockStreamParser() override;

  // StreamParser interface
  MOCK_METHOD8(
      Init,
      void(InitCB init_cb,
           const NewConfigCB& config_cb,
           const NewBuffersCB& new_buffers_cb,
           bool ignore_text_track,
           const EncryptedMediaInitDataCB& encrypted_media_init_data_cb,
           const NewMediaSegmentCB& new_segment_cb,
           const EndMediaSegmentCB& end_of_segment_cb,
           MediaLog* media_log));
  MOCK_METHOD0(Flush, void());
  MOCK_CONST_METHOD0(GetGenerateTimestampsFlag, bool());
  MOCK_METHOD2(Parse, bool(const uint8_t*, int));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockStreamParser);
};

class MockMediaClient : public media::MediaClient {
 public:
  MockMediaClient();
  ~MockMediaClient() override;

  // MediaClient implementation.
  MOCK_METHOD1(AddSupportedKeySystems,
               void(std::vector<std::unique_ptr<media::KeySystemProperties>>*
                        key_systems));
  MOCK_METHOD0(IsKeySystemsUpdateNeeded, bool());
  MOCK_METHOD1(IsSupportedAudioType, bool(const media::AudioType& type));
  MOCK_METHOD1(IsSupportedVideoType, bool(const media::VideoType& type));
  MOCK_METHOD1(IsSupportedBitstreamAudioCodec, bool(media::AudioCodec codec));
  MOCK_METHOD1(GetAudioRendererAlgorithmParameters,
               base::Optional<::media::AudioRendererAlgorithmParameters>(
                   media::AudioParameters audio_parameters));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMediaClient);
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_FILTERS_H_
