// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/test/pipeline_integration_test_base.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/media_tracks.h"
#include "media/base/test_data_util.h"
#include "media/filters/file_data_source.h"
#include "media/filters/memory_data_source.h"
#include "media/media_buildflags.h"
#include "media/renderers/audio_renderer_impl.h"
#include "media/renderers/renderer_impl.h"
#include "media/test/fake_encrypted_media.h"
#include "media/test/mock_media_source.h"
#include "third_party/libaom/av1_buildflags.h"

#if BUILDFLAG(ENABLE_AV1_DECODER)
#include "media/filters/aom_video_decoder.h"
#endif

#if BUILDFLAG(ENABLE_FFMPEG)
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer.h"
#endif

#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
#include "media/filters/ffmpeg_video_decoder.h"
#endif

#if BUILDFLAG(ENABLE_LIBVPX)
#include "media/filters/vpx_video_decoder.h"
#endif

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SaveArg;

namespace media {

static std::vector<std::unique_ptr<VideoDecoder>> CreateVideoDecodersForTest(
    MediaLog* media_log,
    CreateVideoDecodersCB prepend_video_decoders_cb) {
  std::vector<std::unique_ptr<VideoDecoder>> video_decoders;

  if (prepend_video_decoders_cb) {
    video_decoders = prepend_video_decoders_cb.Run();
    DCHECK(!video_decoders.empty());
  }

#if BUILDFLAG(ENABLE_LIBVPX)
  video_decoders.push_back(std::make_unique<OffloadingVpxVideoDecoder>());
#endif

#if BUILDFLAG(ENABLE_AV1_DECODER)
  if (base::FeatureList::IsEnabled(kAv1Decoder))
    video_decoders.push_back(std::make_unique<AomVideoDecoder>(media_log));
#endif

#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  video_decoders.push_back(std::make_unique<FFmpegVideoDecoder>(media_log));
#endif
  return video_decoders;
}

static std::vector<std::unique_ptr<AudioDecoder>> CreateAudioDecodersForTest(
    MediaLog* media_log,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    CreateAudioDecodersCB prepend_audio_decoders_cb) {
  std::vector<std::unique_ptr<AudioDecoder>> audio_decoders;

  if (prepend_audio_decoders_cb) {
    audio_decoders = prepend_audio_decoders_cb.Run();
    DCHECK(!audio_decoders.empty());
  }

#if BUILDFLAG(ENABLE_FFMPEG)
  audio_decoders.push_back(
      std::make_unique<FFmpegAudioDecoder>(media_task_runner, media_log));
#endif
  return audio_decoders;
}

const char kNullVideoHash[] = "d41d8cd98f00b204e9800998ecf8427e";
const char kNullAudioHash[] = "0.00,0.00,0.00,0.00,0.00,0.00,";

class RendererFactoryImpl final : public PipelineTestRendererFactory {
 public:
  explicit RendererFactoryImpl(PipelineIntegrationTestBase* integration_test)
      : integration_test_(integration_test) {}
  ~RendererFactoryImpl() override = default;

  // PipelineTestRendererFactory implementation.
  std::unique_ptr<Renderer> CreateRenderer(
      CreateVideoDecodersCB prepend_video_decoders_cb,
      CreateAudioDecodersCB prepend_audio_decoders_cb) override {
    return integration_test_->CreateRenderer(prepend_video_decoders_cb,
                                             prepend_audio_decoders_cb);
  }

 private:
  PipelineIntegrationTestBase* integration_test_;

  DISALLOW_COPY_AND_ASSIGN(RendererFactoryImpl);
};

PipelineIntegrationTestBase::PipelineIntegrationTestBase()
    : hashing_enabled_(false),
      clockless_playback_(false),
      webaudio_attached_(false),
      pipeline_(
          new PipelineImpl(scoped_task_environment_.GetMainThreadTaskRunner(),
                           scoped_task_environment_.GetMainThreadTaskRunner(),
                           &media_log_)),
      ended_(false),
      pipeline_status_(PIPELINE_OK),
      last_video_frame_format_(PIXEL_FORMAT_UNKNOWN),
      current_duration_(kInfiniteDuration),
      renderer_factory_(new RendererFactoryImpl(this)) {
  ResetVideoHash();
  EXPECT_CALL(*this, OnVideoAverageKeyframeDistanceUpdate()).Times(AnyNumber());
}

PipelineIntegrationTestBase::~PipelineIntegrationTestBase() {
  if (pipeline_->IsRunning())
    Stop();

  demuxer_.reset();
  pipeline_.reset();
  base::RunLoop().RunUntilIdle();
}

void PipelineIntegrationTestBase::ParseTestTypeFlags(uint8_t flags) {
  hashing_enabled_ = flags & kHashed;
  clockless_playback_ = !(flags & kNoClockless);
  webaudio_attached_ = flags & kWebAudio;
  mono_output_ = flags & kMonoOutput;
}

// TODO(xhwang): Method definitions in this file needs to be reordered.

void PipelineIntegrationTestBase::OnSeeked(base::TimeDelta seek_time,
                                           PipelineStatus status) {
  EXPECT_EQ(seek_time, pipeline_->GetMediaTime());
  pipeline_status_ = status;
}

void PipelineIntegrationTestBase::OnStatusCallback(
    const base::Closure& quit_run_loop_closure,
    PipelineStatus status) {
  pipeline_status_ = status;

  if (pipeline_status_ != PIPELINE_OK && pipeline_->IsRunning())
    pipeline_->Stop();

  quit_run_loop_closure.Run();
}

void PipelineIntegrationTestBase::DemuxerEncryptedMediaInitDataCB(
    EmeInitDataType type,
    const std::vector<uint8_t>& init_data) {
  DCHECK(!init_data.empty());
  CHECK(encrypted_media_init_data_cb_);
  encrypted_media_init_data_cb_.Run(type, init_data);
}

void PipelineIntegrationTestBase::DemuxerMediaTracksUpdatedCB(
    std::unique_ptr<MediaTracks> tracks) {
  CHECK(tracks);
  CHECK_GT(tracks->tracks().size(), 0u);

  // Verify that track ids are unique.
  std::set<MediaTrack::Id> track_ids;
  for (const auto& track : tracks->tracks()) {
    EXPECT_EQ(track_ids.end(), track_ids.find(track->id()));
    track_ids.insert(track->id());
  }
}

void PipelineIntegrationTestBase::OnEnded() {
  DCHECK(!ended_);
  ended_ = true;
  pipeline_status_ = PIPELINE_OK;
  if (on_ended_closure_)
    std::move(on_ended_closure_).Run();
}

bool PipelineIntegrationTestBase::WaitUntilOnEnded() {
  EXPECT_EQ(pipeline_status_, PIPELINE_OK);
  PipelineStatus status = WaitUntilEndedOrError();
  EXPECT_TRUE(ended_);
  EXPECT_EQ(pipeline_status_, PIPELINE_OK);
  return ended_ && (status == PIPELINE_OK);
}

PipelineStatus PipelineIntegrationTestBase::WaitUntilEndedOrError() {
  if (!ended_ && pipeline_status_ == PIPELINE_OK) {
    base::RunLoop run_loop;
    RunUntilQuitOrEndedOrError(&run_loop);
  } else {
    scoped_task_environment_.RunUntilIdle();
  }
  return pipeline_status_;
}

void PipelineIntegrationTestBase::OnError(PipelineStatus status) {
  DCHECK_NE(status, PIPELINE_OK);
  pipeline_status_ = status;
  pipeline_->Stop();
  if (on_error_closure_)
    std::move(on_error_closure_).Run();
}

PipelineStatus PipelineIntegrationTestBase::StartInternal(
    std::unique_ptr<DataSource> data_source,
    CdmContext* cdm_context,
    uint8_t test_type,
    CreateVideoDecodersCB prepend_video_decoders_cb,
    CreateAudioDecodersCB prepend_audio_decoders_cb) {
  ParseTestTypeFlags(test_type);

  EXPECT_CALL(*this, OnMetadata(_))
      .Times(AtMost(1))
      .WillRepeatedly(SaveArg<0>(&metadata_));
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH))
      .Times(AnyNumber());
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING))
      .Times(AnyNumber());
  // If the test is expected to have reliable duration information, permit at
  // most two calls to OnDurationChange.  CheckDuration will make sure that no
  // more than one of them is a finite duration.  This allows the pipeline to
  // call back at the end of the media with the known duration.
  //
  // In the event of unreliable duration information, just set the expectation
  // that it's called at least once. Such streams may repeatedly update their
  // duration as new packets are demuxed.
  if (test_type & kUnreliableDuration) {
    EXPECT_CALL(*this, OnDurationChange()).Times(AnyNumber());
  } else {
    EXPECT_CALL(*this, OnDurationChange())
        .Times(AtMost(2))
        .WillRepeatedly(
            Invoke(this, &PipelineIntegrationTestBase::CheckDuration));
  }
  EXPECT_CALL(*this, OnVideoNaturalSizeChange(_)).Times(AnyNumber());
  EXPECT_CALL(*this, OnVideoOpacityChange(_)).WillRepeatedly(Return());
  EXPECT_CALL(*this, OnAudioDecoderChange(_)).Times(AnyNumber());
  EXPECT_CALL(*this, OnVideoDecoderChange(_)).Times(AnyNumber());
  CreateDemuxer(std::move(data_source));

  if (cdm_context) {
    EXPECT_CALL(*this, DecryptorAttached(true));
    pipeline_->SetCdm(
        cdm_context, base::Bind(&PipelineIntegrationTestBase::DecryptorAttached,
                                base::Unretained(this)));
  }

  // Should never be called as the required decryption keys for the encrypted
  // media files are provided in advance.
  EXPECT_CALL(*this, OnWaitingForDecryptionKey()).Times(0);

  // DemuxerStreams may signal config changes.
  // In practice, this doesn't happen for FFmpegDemuxer, but it's allowed for
  // SRC= demuxers in general.
  EXPECT_CALL(*this, OnAudioConfigChange(_)).Times(AnyNumber());
  EXPECT_CALL(*this, OnVideoConfigChange(_)).Times(AnyNumber());

  base::RunLoop run_loop;
  pipeline_->Start(
      Pipeline::StartType::kNormal, demuxer_.get(),
      renderer_factory_->CreateRenderer(prepend_video_decoders_cb,
                                        prepend_audio_decoders_cb),
      this,
      base::Bind(&PipelineIntegrationTestBase::OnStatusCallback,
                 base::Unretained(this), run_loop.QuitWhenIdleClosure()));
  RunUntilQuitOrEndedOrError(&run_loop);
  return pipeline_status_;
}

PipelineStatus PipelineIntegrationTestBase::StartWithFile(
    const std::string& filename,
    CdmContext* cdm_context,
    uint8_t test_type,
    CreateVideoDecodersCB prepend_video_decoders_cb,
    CreateAudioDecodersCB prepend_audio_decoders_cb) {
  std::unique_ptr<FileDataSource> file_data_source(new FileDataSource());
  base::FilePath file_path(GetTestDataFilePath(filename));
  CHECK(file_data_source->Initialize(file_path)) << "Is " << file_path.value()
                                                 << " missing?";
  return StartInternal(std::move(file_data_source), cdm_context, test_type,
                       prepend_video_decoders_cb, prepend_audio_decoders_cb);
}

PipelineStatus PipelineIntegrationTestBase::Start(const std::string& filename) {
  return StartWithFile(filename, nullptr, kNormal);
}

PipelineStatus PipelineIntegrationTestBase::Start(const std::string& filename,
                                                  CdmContext* cdm_context) {
  return StartWithFile(filename, cdm_context, kNormal);
}

PipelineStatus PipelineIntegrationTestBase::Start(
    const std::string& filename,
    uint8_t test_type,
    CreateVideoDecodersCB prepend_video_decoders_cb,
    CreateAudioDecodersCB prepend_audio_decoders_cb) {
  return StartWithFile(filename, nullptr, test_type, prepend_video_decoders_cb,
                       prepend_audio_decoders_cb);
}

PipelineStatus PipelineIntegrationTestBase::Start(const uint8_t* data,
                                                  size_t size,
                                                  uint8_t test_type) {
  return StartInternal(std::make_unique<MemoryDataSource>(data, size), nullptr,
                       test_type);
}

void PipelineIntegrationTestBase::Play() {
  pipeline_->SetPlaybackRate(1);
}

void PipelineIntegrationTestBase::Pause() {
  pipeline_->SetPlaybackRate(0);
}

bool PipelineIntegrationTestBase::Seek(base::TimeDelta seek_time) {
  // Enforce that BUFFERING_HAVE_ENOUGH is the first call below.
  ::testing::InSequence dummy;

  ended_ = false;
  base::RunLoop run_loop;

  // Should always transition to HAVE_ENOUGH once the seek completes.
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  // After initial HAVE_ENOUGH, any buffering state change is allowed as
  // playback may cause any number of underflow/preroll events.
  EXPECT_CALL(*this, OnBufferingStateChange(_)).Times(AnyNumber());

  pipeline_->Seek(seek_time, base::Bind(&PipelineIntegrationTestBase::OnSeeked,
                                        base::Unretained(this), seek_time));
  RunUntilQuitOrError(&run_loop);
  return (pipeline_status_ == PIPELINE_OK);
}

bool PipelineIntegrationTestBase::Suspend() {
  base::RunLoop run_loop;
  pipeline_->Suspend(base::Bind(&PipelineIntegrationTestBase::OnStatusCallback,
                                base::Unretained(this),
                                run_loop.QuitWhenIdleClosure()));
  RunUntilQuitOrError(&run_loop);
  return (pipeline_status_ == PIPELINE_OK);
}

bool PipelineIntegrationTestBase::Resume(base::TimeDelta seek_time) {
  ended_ = false;

  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  pipeline_->Resume(renderer_factory_->CreateRenderer(CreateVideoDecodersCB(),
                                                      CreateAudioDecodersCB()),
                    seek_time,
                    base::Bind(&PipelineIntegrationTestBase::OnSeeked,
                               base::Unretained(this), seek_time));
  RunUntilQuitOrError(&run_loop);
  return (pipeline_status_ == PIPELINE_OK);
}

void PipelineIntegrationTestBase::Stop() {
  DCHECK(pipeline_->IsRunning());
  pipeline_->Stop();
  base::RunLoop().RunUntilIdle();
}

void PipelineIntegrationTestBase::FailTest(PipelineStatus status) {
  DCHECK_NE(PIPELINE_OK, status);
  OnError(status);
}

void PipelineIntegrationTestBase::QuitAfterCurrentTimeTask(
    base::TimeDelta quit_time,
    base::OnceClosure quit_closure) {
  if (!pipeline_ || pipeline_->GetMediaTime() >= quit_time ||
      pipeline_status_ != PIPELINE_OK) {
    std::move(quit_closure).Run();
    return;
  }

  scoped_task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PipelineIntegrationTestBase::QuitAfterCurrentTimeTask,
                     base::Unretained(this), quit_time,
                     std::move(quit_closure)),
      base::TimeDelta::FromMilliseconds(10));
}

bool PipelineIntegrationTestBase::WaitUntilCurrentTimeIsAfter(
    const base::TimeDelta& wait_time) {
  DCHECK(pipeline_->IsRunning());
  DCHECK_GT(pipeline_->GetPlaybackRate(), 0);
  DCHECK(wait_time <= pipeline_->GetMediaDuration());

  base::RunLoop run_loop;
  scoped_task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PipelineIntegrationTestBase::QuitAfterCurrentTimeTask,
                     base::Unretained(this), wait_time,
                     run_loop.QuitWhenIdleClosure()),
      base::TimeDelta::FromMilliseconds(10));

  RunUntilQuitOrEndedOrError(&run_loop);

  return (pipeline_status_ == PIPELINE_OK);
}

void PipelineIntegrationTestBase::CreateDemuxer(
    std::unique_ptr<DataSource> data_source) {
  data_source_ = std::move(data_source);

#if BUILDFLAG(ENABLE_FFMPEG)
  demuxer_ = std::unique_ptr<Demuxer>(new FFmpegDemuxer(
      scoped_task_environment_.GetMainThreadTaskRunner(), data_source_.get(),
      base::BindRepeating(
          &PipelineIntegrationTestBase::DemuxerEncryptedMediaInitDataCB,
          base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::DemuxerMediaTracksUpdatedCB,
                 base::Unretained(this)),
      &media_log_, true));
#endif
}

std::unique_ptr<Renderer> PipelineIntegrationTestBase::CreateRenderer(
    CreateVideoDecodersCB prepend_video_decoders_cb,
    CreateAudioDecodersCB prepend_audio_decoders_cb) {
  // Simulate a 60Hz rendering sink.
  video_sink_.reset(new NullVideoSink(
      clockless_playback_, base::TimeDelta::FromSecondsD(1.0 / 60),
      base::Bind(&PipelineIntegrationTestBase::OnVideoFramePaint,
                 base::Unretained(this)),
      scoped_task_environment_.GetMainThreadTaskRunner()));

  // Disable frame dropping if hashing is enabled.
  std::unique_ptr<VideoRenderer> video_renderer(new VideoRendererImpl(
      scoped_task_environment_.GetMainThreadTaskRunner(), video_sink_.get(),
      base::Bind(&CreateVideoDecodersForTest, &media_log_,
                 prepend_video_decoders_cb),
      false, &media_log_, nullptr));

  if (!clockless_playback_) {
    DCHECK(!mono_output_) << " NullAudioSink doesn't specify output parameters";

    audio_sink_ =
        new NullAudioSink(scoped_task_environment_.GetMainThreadTaskRunner());
  } else {
    ChannelLayout output_layout =
        mono_output_ ? CHANNEL_LAYOUT_MONO : CHANNEL_LAYOUT_STEREO;

    clockless_audio_sink_ = new ClocklessAudioSink(
        OutputDeviceInfo("", OUTPUT_DEVICE_STATUS_OK,
                         AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                         output_layout, 44100, 512)));

    // Say "not optimized for hardware parameters" to disallow renderer
    // resampling. Hashed tests need this avoid platform dependent floating
    // point precision differences.
    if (webaudio_attached_ || hashing_enabled_) {
      clockless_audio_sink_->SetIsOptimizedForHardwareParametersForTesting(
          false);
    }
  }

  std::unique_ptr<AudioRenderer> audio_renderer(new AudioRendererImpl(
      scoped_task_environment_.GetMainThreadTaskRunner(),
      (clockless_playback_)
          ? static_cast<AudioRendererSink*>(clockless_audio_sink_.get())
          : audio_sink_.get(),
      base::Bind(&CreateAudioDecodersForTest, &media_log_,
                 scoped_task_environment_.GetMainThreadTaskRunner(),
                 prepend_audio_decoders_cb),
      &media_log_));
  if (hashing_enabled_) {
    if (clockless_playback_)
      clockless_audio_sink_->StartAudioHashForTesting();
    else
      audio_sink_->StartAudioHashForTesting();
  }

  static_cast<AudioRendererImpl*>(audio_renderer.get())
      ->SetPlayDelayCBForTesting(std::move(audio_play_delay_cb_));

  std::unique_ptr<RendererImpl> renderer_impl(
      new RendererImpl(scoped_task_environment_.GetMainThreadTaskRunner(),
                       std::move(audio_renderer), std::move(video_renderer)));

  // Prevent non-deterministic buffering state callbacks from firing (e.g., slow
  // machine, valgrind).
  renderer_impl->DisableUnderflowForTesting();

  if (clockless_playback_)
    renderer_impl->EnableClocklessVideoPlaybackForTesting();

  return std::move(renderer_impl);
}

void PipelineIntegrationTestBase::OnVideoFramePaint(
    const scoped_refptr<VideoFrame>& frame) {
  last_video_frame_format_ = frame->format();
  last_video_frame_color_space_ = frame->ColorSpace();
  if (!hashing_enabled_ || last_frame_ == frame)
    return;
  last_frame_ = frame;
  DVLOG(3) << __func__ << " pts=" << frame->timestamp().InSecondsF();
  VideoFrame::HashFrameForTesting(&md5_context_, frame);
}

void PipelineIntegrationTestBase::CheckDuration() {
  // Allow the pipeline to specify indefinite duration, then reduce it once
  // it becomes known.
  ASSERT_EQ(kInfiniteDuration, current_duration_);
  base::TimeDelta new_duration = pipeline_->GetMediaDuration();
  current_duration_ = new_duration;
}

base::TimeDelta PipelineIntegrationTestBase::GetStartTime() {
  return demuxer_->GetStartTime();
}

void PipelineIntegrationTestBase::ResetVideoHash() {
  DVLOG(1) << __func__;
  base::MD5Init(&md5_context_);
}

std::string PipelineIntegrationTestBase::GetVideoHash() {
  DCHECK(hashing_enabled_);
  base::MD5Digest digest;
  base::MD5Final(&digest, &md5_context_);
  return base::MD5DigestToBase16(digest);
}

std::string PipelineIntegrationTestBase::GetAudioHash() {
  DCHECK(hashing_enabled_);

  if (clockless_playback_)
    return clockless_audio_sink_->GetAudioHashForTesting();
  return audio_sink_->GetAudioHashForTesting();
}

base::TimeDelta PipelineIntegrationTestBase::GetAudioTime() {
  DCHECK(clockless_playback_);
  return clockless_audio_sink_->render_time();
}

PipelineStatus PipelineIntegrationTestBase::StartPipelineWithMediaSource(
    MockMediaSource* source) {
  return StartPipelineWithMediaSource(source, kNormal, nullptr);
}

PipelineStatus PipelineIntegrationTestBase::StartPipelineWithEncryptedMedia(
    MockMediaSource* source,
    FakeEncryptedMedia* encrypted_media) {
  return StartPipelineWithMediaSource(source, kNormal, encrypted_media);
}

PipelineStatus PipelineIntegrationTestBase::StartPipelineWithMediaSource(
    MockMediaSource* source,
    uint8_t test_type,
    FakeEncryptedMedia* encrypted_media) {
  ParseTestTypeFlags(test_type);

  if (!(test_type & kExpectDemuxerFailure))
    EXPECT_CALL(*source, InitSegmentReceivedMock(_)).Times(AtLeast(1));

  EXPECT_CALL(*this, OnMetadata(_))
      .Times(AtMost(1))
      .WillRepeatedly(SaveArg<0>(&metadata_));
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH))
      .Times(AnyNumber());
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING))
      .Times(AnyNumber());
  EXPECT_CALL(*this, OnDurationChange()).Times(AnyNumber());
  EXPECT_CALL(*this, OnVideoNaturalSizeChange(_)).Times(AnyNumber());
  EXPECT_CALL(*this, OnVideoOpacityChange(_)).Times(AtMost(1));
  EXPECT_CALL(*this, OnAudioDecoderChange(_)).Times(AnyNumber());
  EXPECT_CALL(*this, OnVideoDecoderChange(_)).Times(AnyNumber());

  base::RunLoop run_loop;

  source->set_demuxer_failure_cb(
      base::Bind(&PipelineIntegrationTestBase::OnStatusCallback,
                 base::Unretained(this), run_loop.QuitWhenIdleClosure()));
  demuxer_ = source->GetDemuxer();

  // DemuxerStreams may signal config changes.
  // Config change tests should set more specific expectations about the number
  // of calls.
  EXPECT_CALL(*this, OnAudioConfigChange(_)).Times(AnyNumber());
  EXPECT_CALL(*this, OnVideoConfigChange(_)).Times(AnyNumber());

  if (encrypted_media) {
    EXPECT_CALL(*this, DecryptorAttached(true));

    // Encrypted content used but keys provided in advance, so this is
    // never called.
    EXPECT_CALL(*this, OnWaitingForDecryptionKey()).Times(0);
    pipeline_->SetCdm(
        encrypted_media->GetCdmContext(),
        base::Bind(&PipelineIntegrationTestBase::DecryptorAttached,
                   base::Unretained(this)));
  } else {
    // Encrypted content not used, so this is never called.
    EXPECT_CALL(*this, OnWaitingForDecryptionKey()).Times(0);
  }

  pipeline_->Start(
      Pipeline::StartType::kNormal, demuxer_.get(),
      renderer_factory_->CreateRenderer(CreateVideoDecodersCB(),
                                        CreateAudioDecodersCB()),
      this,
      base::Bind(&PipelineIntegrationTestBase::OnStatusCallback,
                 base::Unretained(this), run_loop.QuitWhenIdleClosure()));

  if (encrypted_media) {
    source->set_encrypted_media_init_data_cb(
        base::Bind(&FakeEncryptedMedia::OnEncryptedMediaInitData,
                   base::Unretained(encrypted_media)));
  }

  RunUntilQuitOrEndedOrError(&run_loop);

  for (auto* stream : demuxer_->GetAllStreams()) {
    EXPECT_TRUE(stream->SupportsConfigChanges());
  }

  return pipeline_status_;
}

void PipelineIntegrationTestBase::RunUntilQuitOrError(base::RunLoop* run_loop) {
  // We always install an error handler to avoid test hangs.
  on_error_closure_ = run_loop->QuitWhenIdleClosure();

  run_loop->Run();
  on_ended_closure_ = base::OnceClosure();
  on_error_closure_ = base::OnceClosure();
  scoped_task_environment_.RunUntilIdle();
}

void PipelineIntegrationTestBase::RunUntilQuitOrEndedOrError(
    base::RunLoop* run_loop) {
  DCHECK(on_ended_closure_.is_null());
  DCHECK(on_error_closure_.is_null());

  on_ended_closure_ = run_loop->QuitWhenIdleClosure();
  RunUntilQuitOrError(run_loop);
}

base::TimeTicks DummyTickClock::NowTicks() const {
  now_ += base::TimeDelta::FromSeconds(60);
  return now_;
}

}  // namespace media
