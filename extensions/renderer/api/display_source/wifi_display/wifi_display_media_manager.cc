// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/display_source/wifi_display/wifi_display_media_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/video_encode_accelerator.h"
#include "extensions/common/mojom/wifi_display_session_service.mojom.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_elementary_stream_info.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_media_pipeline.h"
#include "media/base/bind_to_current_loop.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace extensions {

namespace {

const char kErrorNoVideoFormatData[] =
    "Failed to get video format data from the given MediaStreamTrack object";
const char kErrorSinkCannotPlayVideo[] =
    "The sink cannot play video from the given MediaStreamTrack object";
const char kErrorSinkCannotPlayAudio[] =
    "The sink cannot play audio from the given MediaStreamTrack object";
const char kErrorMediaPipelineFailure[] =
    "Failed to initialize media pipeline for the session";
}  // namespace

class WiFiDisplayAudioSink {
 public:
  WiFiDisplayAudioSink(const blink::WebMediaStreamTrack& track,
                       blink::WebMediaStreamAudioSink* delegate)
      : track_(track), delegate_(delegate), sink_added_(false) {}

  ~WiFiDisplayAudioSink() { Stop(); }

  void Start() {
    DCHECK(!sink_added_);
    sink_added_ = true;
    delegate_->AddToAudioTrack(delegate_, track_);
  }

  void Stop() {
    if (sink_added_) {
      delegate_->RemoveFromAudioTrack(delegate_, track_);
      sink_added_ = false;
    }
  }

 private:
  blink::WebMediaStreamTrack track_;
  blink::WebMediaStreamAudioSink* delegate_;
  bool sink_added_;
};

class WiFiDisplayVideoSink : public content::MediaStreamVideoSink {
 public:
  WiFiDisplayVideoSink(
      const blink::WebMediaStreamTrack& track,
      const content::VideoCaptureDeliverFrameCB& callback)
      : track_(track),
        callback_(callback) {
  }

  ~WiFiDisplayVideoSink() override {
    DisconnectFromTrack();
  }

  void Start() {
    // Callback is invoked on IO thread.
    ConnectToTrack(track_, callback_, false);
  }

 private:
  blink::WebMediaStreamTrack track_;
  content::VideoCaptureDeliverFrameCB callback_;
  DISALLOW_COPY_AND_ASSIGN(WiFiDisplayVideoSink);
};

WiFiDisplayMediaManager::WiFiDisplayMediaManager(
    const blink::WebMediaStreamTrack& video_track,
    const blink::WebMediaStreamTrack& audio_track,
    const net::IPAddress& sink_ip_address,
    service_manager::InterfaceProvider* interface_provider,
    const ErrorCallback& error_callback)
    : video_track_(video_track),
      audio_track_(audio_track),
      interface_provider_(interface_provider),
      sink_ip_address_(sink_ip_address),
      player_(nullptr),
      io_task_runner_(content::RenderThread::Get()->GetIOTaskRunner()),
      error_callback_(error_callback),
      is_playing_(false),
      is_initialized_(false),
      weak_factory_(this) {
  DCHECK(!video_track.isNull() || !audio_track.isNull());
  DCHECK(interface_provider_);
  DCHECK(!error_callback_.is_null());
}

WiFiDisplayMediaManager::~WiFiDisplayMediaManager() {
  Teardown();
}

void WiFiDisplayMediaManager::Play() {
  is_playing_ = true;
  if (!player_) {
    auto service_callback = base::Bind(
        &WiFiDisplayMediaManager::RegisterMediaService,
        base::Unretained(this),
        base::ThreadTaskRunnerHandle::Get());
    base::PostTaskAndReplyWithResult(io_task_runner_.get(), FROM_HERE,
        base::Bind(
            &WiFiDisplayMediaPipeline::Create,
            GetSessionType(),
            video_encoder_parameters_,
            optimal_audio_codec_,
            sink_ip_address_,
            sink_rtp_ports_,
            service_callback,  // To be invoked on IO thread.
            media::BindToCurrentLoop(error_callback_)),
        base::Bind(&WiFiDisplayMediaManager::OnPlayerCreated,
                   weak_factory_.GetWeakPtr()));
    return;
  }

  if (!is_initialized_) {
    return;  // Waiting for initialization being completed.
  }

  if (!audio_track_.isNull()) {
    audio_sink_.reset(
        new WiFiDisplayAudioSink(audio_track_, player_->audio_sink()));
    audio_sink_->Start();
  }

  if (!video_track_.isNull()) {
    // To be called on IO thread.
    auto on_raw_video_frame = base::Bind(
        &WiFiDisplayMediaPipeline::InsertRawVideoFrame,
        base::Unretained(player_));
    video_sink_.reset(
        new WiFiDisplayVideoSink(video_track_, on_raw_video_frame));
    video_sink_->Start();
  }
}

void WiFiDisplayMediaManager::Teardown() {
  Pause();
  if (player_) {
    io_task_runner_->DeleteSoon(FROM_HERE, player_);
    player_ = nullptr;
  }
  is_initialized_ = false;
  session_id_.clear();
}

void WiFiDisplayMediaManager::Pause() {
  is_playing_ = false;
  audio_sink_.reset();
  video_sink_.reset();
}

bool WiFiDisplayMediaManager::IsPaused() const {
  return !is_playing_;
}

wds::SessionType WiFiDisplayMediaManager::GetSessionType() const {
  uint16_t session_type = 0;
  if (!video_track_.isNull())
    session_type |= wds::VideoSession;

  if (!audio_track_.isNull())
    session_type |= wds::AudioSession;

  return static_cast<wds::SessionType>(session_type);
}

void WiFiDisplayMediaManager::SetSinkRtpPorts(int port1, int port2) {
  sink_rtp_ports_ = std::pair<int, int>(port1, port2);
}

std::pair<int, int> WiFiDisplayMediaManager::GetSinkRtpPorts() const {
  return sink_rtp_ports_;
}

int WiFiDisplayMediaManager::GetLocalRtpPort() const {
  NOTIMPLEMENTED();
  return 0;
}

namespace {
struct VideoFormat {
  wds::RateAndResolution rr;
  int width;
  int height;
  int frame_rate;
};

const VideoFormat cea_table[] = {
  {wds::CEA640x480p60, 640, 480, 60},
  {wds::CEA720x480p60, 720, 480, 60},
  {wds::CEA720x576p50, 720, 576, 50},
  {wds::CEA1280x720p30, 1280, 720, 30},
  {wds::CEA1280x720p60, 1280, 720, 60},
  {wds::CEA1920x1080p30, 1920, 1080, 30},
  {wds::CEA1920x1080p60, 1920, 1080, 60},
  {wds::CEA1280x720p25, 1280, 720, 25},
  {wds::CEA1280x720p50, 1280, 720, 50},
  {wds::CEA1920x1080p25, 1920, 1080, 25},
  {wds::CEA1920x1080p50, 1920, 1080, 50},
  {wds::CEA1280x720p24, 1280, 720, 24},
  {wds::CEA1920x1080p24, 1920, 1080, 24}
};

const VideoFormat vesa_table[] = {
  {wds::VESA800x600p30,    800, 600, 30},
  {wds::VESA800x600p60,    800, 600, 60},
  {wds::VESA1024x768p30,   1024, 768, 30},
  {wds::VESA1024x768p60,   1024, 768, 60},
  {wds::VESA1152x864p30,   1152, 864, 30},
  {wds::VESA1152x864p60,   1152, 864, 60},
  {wds::VESA1280x768p30,   1280, 768, 30},
  {wds::VESA1280x768p60,   1280, 768, 60},
  {wds::VESA1280x800p30,   1280, 800, 30},
  {wds::VESA1280x800p60,   1280, 800, 60},
  {wds::VESA1360x768p30,   1360, 768, 30},
  {wds::VESA1360x768p60,   1360, 768, 60},
  {wds::VESA1366x768p30,   1366, 768, 30},
  {wds::VESA1366x768p60,   1366, 768, 60},
  {wds::VESA1280x1024p30,  1280, 1024, 30},
  {wds::VESA1280x1024p60,  1280, 1024, 60},
  {wds::VESA1400x1050p30,  1400, 1050, 30},
  {wds::VESA1400x1050p60,  1400, 1050, 60},
  {wds::VESA1440x900p30,   1440, 900, 30},
  {wds::VESA1440x900p60,   1440, 900, 60},
  {wds::VESA1600x900p30,   1600, 900, 30},
  {wds::VESA1600x900p60,   1600, 900, 60},
  {wds::VESA1600x1200p30,  1600, 1200, 30},
  {wds::VESA1600x1200p60,  1600, 1200, 60},
  {wds::VESA1680x1024p30,  1680, 1024, 30},
  {wds::VESA1680x1024p60,  1680, 1024, 60},
  {wds::VESA1680x1050p30,  1680, 1050, 30},
  {wds::VESA1680x1050p60,  1680, 1050, 60},
  {wds::VESA1920x1200p30,  1920, 1200, 30}
};

const VideoFormat hh_table[] = {
  {wds::HH800x480p30, 800, 480, 30},
  {wds::HH800x480p60, 800, 480, 60},
  {wds::HH854x480p30, 854, 480, 30},
  {wds::HH854x480p60, 854, 480, 60},
  {wds::HH864x480p30, 864, 480, 30},
  {wds::HH864x480p60, 864, 480, 60},
  {wds::HH640x360p30, 640, 360, 30},
  {wds::HH640x360p60, 640, 360, 60},
  {wds::HH960x540p30, 960, 540, 30},
  {wds::HH960x540p60, 960, 540, 60},
  {wds::HH848x480p30, 848, 480, 30},
  {wds::HH848x480p60, 848, 480, 60}
};

template <wds::ResolutionType type, unsigned N>
bool FindRateResolution(const media::VideoCaptureFormat* format,
                        const wds::RateAndResolutionsBitmap& bitmap,
                        const VideoFormat (&table)[N],
                        wds::H264VideoFormat* result /*out*/) {
  for (unsigned i = 0; i < N; ++i) {
     if (bitmap.test(table[i].rr)) {
        if (format->frame_size.width() == table[i].width &&
            format->frame_size.height() == table[i].height &&
            format->frame_rate == table[i].frame_rate) {
          result->rate_resolution = table[i].rr;
          result->type = type;
          return true;
        }
     }
  }
  return false;
}

void FindCompatibleFormats(
    const media::VideoCaptureFormat* capture_format,
    const std::vector<wds::H264VideoCodec>& sink_supported_codecs,
    std::vector<wds::H264VideoFormat>* result /*out*/) {
  DCHECK(result);
  for (const wds::H264VideoCodec& codec : sink_supported_codecs) {
    wds::H264VideoFormat format;
    bool found =
        FindRateResolution<wds::CEA>(
            capture_format, codec.cea_rr, cea_table, &format) ||
        FindRateResolution<wds::VESA>(
            capture_format, codec.vesa_rr, vesa_table, &format) ||
        FindRateResolution<wds::HH>(
            capture_format, codec.hh_rr, hh_table, &format);
    if (found) {
      format.profile = codec.profile;
      format.level = codec.level;
      result->emplace_back(format);
    }
  }
}

}  // namespace

wds::H264VideoFormat WiFiDisplayMediaManager::GetOptimalVideoFormat() const {
  return optimal_video_format_;
}

namespace {

int GetBitRate(const gfx::Size& frame_size) {
  DCHECK_GE(frame_size.height(), 360);
  if (frame_size.height() < 720)
    return 2500000;
  if (frame_size.height() < 1080)
    return 5000000;
  return 8000000;
}

void CreateVideoEncodeMemory(
    size_t size,
    const WiFiDisplayVideoEncoder::ReceiveEncodeMemoryCallback& callback) {
  DCHECK(content::RenderThread::Get());

  base::UnsafeSharedMemoryRegion shm =
      mojo::CreateUnsafeSharedMemoryRegion(size);
  if (!shm.IsValid()) {
    NOTREACHED() << "Shared memory allocation or map failed";
  }
  callback.Run(std::move(shm));
}

}  // namespace

bool WiFiDisplayMediaManager::InitOptimalVideoFormat(
    const wds::NativeVideoFormat& sink_native_format,
    const std::vector<wds::H264VideoCodec>& sink_supported_codecs) {
  const media::VideoCaptureFormat* capture_format =
      content::GetCurrentVideoTrackFormat(video_track_);
  if (!capture_format) {
    error_callback_.Run(kErrorNoVideoFormatData);
    return false;
  }

  std::vector<wds::H264VideoFormat> compatible_formats;

  FindCompatibleFormats(
      capture_format, sink_supported_codecs, &compatible_formats);

  if (compatible_formats.empty()) {
    error_callback_.Run(kErrorSinkCannotPlayVideo);
    return false;
  }

  // The found compatible formats have the same frame rate and resolution but
  // different video encoder profiles. Pick the appropriate profile from the
  // supported by video encoder.
  std::vector<wds::H264Profile> supported_profiles =
      WiFiDisplayVideoEncoder::FindSupportedProfiles(
          capture_format->frame_size,
          capture_format->frame_rate);

  if (supported_profiles.empty()) {
    error_callback_.Run(kErrorSinkCannotPlayVideo);
    return false;
  }

  bool profile_found = false;
  for (wds::H264Profile profile : supported_profiles) {
    if (profile_found)
      break;

    for (const auto& format : compatible_formats) {
      if (format.profile == profile) {
         optimal_video_format_ = format;
         profile_found = true;
         break;
      }
    }
  }

  if (!profile_found) {
    error_callback_.Run(kErrorSinkCannotPlayVideo);
    return false;
  }

  video_encoder_parameters_.frame_size = capture_format->frame_size;
  video_encoder_parameters_.frame_rate =
      static_cast<int>(capture_format->frame_rate);
  video_encoder_parameters_.bit_rate = GetBitRate(capture_format->frame_size);
  video_encoder_parameters_.profile = optimal_video_format_.profile;
  video_encoder_parameters_.level = optimal_video_format_.level;
  video_encoder_parameters_.create_memory_callback =
      media::BindToCurrentLoop(base::Bind(&CreateVideoEncodeMemory));
  video_encoder_parameters_.vea_create_callback =
      media::BindToCurrentLoop(
          base::Bind(&content::CreateVideoEncodeAccelerator));

  return true;
}

bool WiFiDisplayMediaManager::InitOptimalAudioFormat(
    const std::vector<wds::AudioCodec>& sink_codecs) {
  for (const wds::AudioCodec& codec : sink_codecs) {
    // MediaStreamTrack contains LPCM audio.
    if (codec.format == wds::LPCM) {
      optimal_audio_codec_ = codec;
      // Picking a single mode.
      wds::AudioModes optimal_mode;
      if (codec.modes.test(wds::LPCM_44_1K_16B_2CH))
        optimal_mode.set(wds::LPCM_44_1K_16B_2CH);
      else
        optimal_mode.set(wds::LPCM_48K_16B_2CH);
      optimal_audio_codec_.modes = optimal_mode;
      return true;
    }
  }
  error_callback_.Run(kErrorSinkCannotPlayAudio);
  return false;
}

wds::AudioCodec WiFiDisplayMediaManager::GetOptimalAudioFormat() const {
  return optimal_audio_codec_;
}

void WiFiDisplayMediaManager::SendIDRPicture() {
  DCHECK(player_);
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WiFiDisplayMediaPipeline::RequestIDRPicture,
                                base::Unretained(player_)));
}

std::string WiFiDisplayMediaManager::GetSessionId() const {
  if (session_id_.empty())
    session_id_ = base::RandBytesAsString(8);
  return session_id_;
}

void WiFiDisplayMediaManager::OnPlayerCreated(
    std::unique_ptr<WiFiDisplayMediaPipeline> player) {
  DCHECK(player);
  DCHECK(content::RenderThread::Get());
  player_ = player.release();

  auto completion_callback = base::Bind(
     &WiFiDisplayMediaManager::OnMediaPipelineInitialized,
     weak_factory_.GetWeakPtr());

  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WiFiDisplayMediaPipeline::Initialize,
                                base::Unretained(player_),
                                media::BindToCurrentLoop(completion_callback)));
}

void WiFiDisplayMediaManager::OnMediaPipelineInitialized(bool success) {
  DCHECK(content::RenderThread::Get());
  is_initialized_ = success;

  if (!is_initialized_) {
    error_callback_.Run(kErrorMediaPipelineFailure);
    return;
  }

  if (is_playing_)
    Play();
}

// Note: invoked on IO thread
void WiFiDisplayMediaManager::RegisterMediaService(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_runner,
    mojo::PendingReceiver<mojom::WiFiDisplayMediaService> receiver,
    const base::Closure& on_completed) {
  auto connect_service_callback =
      base::Bind(&WiFiDisplayMediaManager::ConnectToRemoteService,
                 base::Unretained(this), base::Passed(&receiver));
  main_runner->PostTaskAndReply(FROM_HERE,
      connect_service_callback,
      media::BindToCurrentLoop(on_completed));
}

void WiFiDisplayMediaManager::ConnectToRemoteService(
    mojo::PendingReceiver<mojom::WiFiDisplayMediaService> receiver) {
  DCHECK(content::RenderThread::Get());
  interface_provider_->GetInterface(std::move(receiver));
}

}  // namespace extensions
