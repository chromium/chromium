// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_video_encoder.h"
#include "third_party/openh264/src/codec/api/svc/codec_api.h"
#include "third_party/openh264/src/codec/api/svc/codec_app_def.h"
#include "third_party/openh264/src/codec/api/svc/codec_def.h"

namespace extensions {

namespace {

size_t CalculateLayerBitStreamLength(const SLayerBSInfo& layer_info) {
  size_t length = 0u;
  for (int i = 0; i < layer_info.iNalCount; ++i)
    length += static_cast<size_t>(layer_info.pNalLengthInByte[i]);
  return length;
}

size_t CalculateFrameBitStreamLength(const SFrameBSInfo& frame_info) {
  size_t length = 0u;
  for (int i = 0; i < frame_info.iLayerNum; ++i)
    length += CalculateLayerBitStreamLength(frame_info.sLayerInfo[i]);
  return length;
}

// This video encoder implements software H.264 video encoding using OpenH264
// library.
class WiFiDisplayVideoEncoderSVC final : public WiFiDisplayVideoEncoder {
 public:
  static void Create(const InitParameters& params,
                     VideoEncoderCallback encoder_callback);

 private:
  WiFiDisplayVideoEncoderSVC(
      scoped_refptr<base::SingleThreadTaskRunner> client_task_runner,
      std::unique_ptr<base::Thread> media_thread);
  ~WiFiDisplayVideoEncoderSVC() override;

  scoped_refptr<WiFiDisplayVideoEncoder> InitOnMediaThread(
      const InitParameters& params);
  void InsertFrameOnMediaThread(scoped_refptr<media::VideoFrame> video_frame,
                                base::TimeTicks reference_time,
                                bool send_idr) override;

  scoped_refptr<base::SingleThreadTaskRunner> client_task_runner_;
  std::unique_ptr<base::Thread> media_thread_;
  ISVCEncoder* openh264_encoder_;
  base::TimeTicks start_time_;
};

// static
void WiFiDisplayVideoEncoderSVC::Create(const InitParameters& params,
                                        VideoEncoderCallback encoder_callback) {
  // TODO(e_hakkinen): Use normal media thread once it is exposed to extensions
  // and can be passed to this class.
  std::unique_ptr<base::Thread> media_thread(
      new base::Thread("WiFiDisplaySVCMedia"));
  media_thread->Start();

  base::PostTaskAndReplyWithResult(
      media_thread->task_runner().get(), FROM_HERE,
      base::BindOnce(
          &WiFiDisplayVideoEncoderSVC::InitOnMediaThread,
          base::WrapRefCounted(new WiFiDisplayVideoEncoderSVC(
              base::ThreadTaskRunnerHandle::Get(), std::move(media_thread))),
          params),
      base::BindOnce(std::move(encoder_callback)));
}

WiFiDisplayVideoEncoderSVC::WiFiDisplayVideoEncoderSVC(
    scoped_refptr<base::SingleThreadTaskRunner> client_task_runner,
    std::unique_ptr<base::Thread> media_thread)
    : WiFiDisplayVideoEncoder(media_thread->task_runner()),
      client_task_runner_(std::move(client_task_runner)),
      media_thread_(std::move(media_thread)),
      openh264_encoder_(nullptr) {}

WiFiDisplayVideoEncoderSVC::~WiFiDisplayVideoEncoderSVC() {
  if (openh264_encoder_) {
    if (int err = openh264_encoder_->Uninitialize()) {
      DVLOG(1) << "Failed to uninit OpenH264 encoder: error=" << err;
    }
    WelsDestroySVCEncoder(openh264_encoder_);
  }
  client_task_runner_->DeleteSoon(FROM_HERE, media_thread_.release());
}

scoped_refptr<WiFiDisplayVideoEncoder>
WiFiDisplayVideoEncoderSVC::InitOnMediaThread(const InitParameters& params) {
  DCHECK(!openh264_encoder_);

  if (int err = WelsCreateSVCEncoder(&openh264_encoder_)) {
    DVLOG(1) << "Failed to create OpenH264 encoder: error=" << err;
    return nullptr;
  }

  SEncParamExt svc_params;
  if (int err = openh264_encoder_->GetDefaultParams(&svc_params)) {
    DVLOG(1) << "Failed to get default OpenH264 parameters: error=" << err;
    return nullptr;
  }

  svc_params.fMaxFrameRate = params.frame_rate;
  svc_params.iPicHeight = params.frame_size.height();
  svc_params.iPicWidth = params.frame_size.width();
  svc_params.iTargetBitrate = params.bit_rate;
  svc_params.iUsageType = SCREEN_CONTENT_REAL_TIME;
  svc_params.sSpatialLayers[0].fFrameRate = svc_params.fMaxFrameRate;
  svc_params.sSpatialLayers[0].iMaxSpatialBitrate = svc_params.iTargetBitrate;
  svc_params.sSpatialLayers[0].iSpatialBitrate = svc_params.iTargetBitrate;
  svc_params.sSpatialLayers[0].iVideoHeight = svc_params.iPicHeight;
  svc_params.sSpatialLayers[0].iVideoWidth = svc_params.iPicWidth;

  if (int err = openh264_encoder_->InitializeExt(&svc_params)) {
    DVLOG(1) << "Failed to init OpenH264 encoder: error=" << err;
    return nullptr;
  }

  int video_format = EVideoFormatType::videoFormatI420;
  if (int err = openh264_encoder_->SetOption(ENCODER_OPTION_DATAFORMAT,
                                             &video_format)) {
    DVLOG(1) << "Failed to set data format for OpenH264 encoder: error=" << err;
    return nullptr;
  }

  return this;
}

void WiFiDisplayVideoEncoderSVC::InsertFrameOnMediaThread(
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks reference_time,
    bool send_idr) {
  DCHECK_EQ(media::PIXEL_FORMAT_I420, video_frame->format());

  if (start_time_.is_null())
    start_time_ = reference_time;

  SSourcePicture picture;
  std::memset(&picture, 0, sizeof(picture));
  picture.iColorFormat = EVideoFormatType::videoFormatI420;
  picture.iPicHeight = video_frame->coded_size().height();
  picture.iPicWidth = video_frame->coded_size().width();
  picture.uiTimeStamp = (reference_time - start_time_).InMilliseconds();
  for (size_t plane_count = video_frame->NumPlanes(video_frame->format()),
              plane = 0u;
       plane < plane_count; ++plane) {
    picture.pData[plane] = video_frame->data(plane);
    picture.iStride[plane] = video_frame->stride(plane);
  }

  if (send_idr) {
    if (int err = openh264_encoder_->ForceIntraFrame(true)) {
      DVLOG(1) << "Failed to force intra frame using OpenH264 encoder: error="
               << err;
    }
  }

  SFrameBSInfo info;
  std::memset(&info, 0, sizeof(info));
  if (int err = openh264_encoder_->EncodeFrame(&picture, &info)) {
    DVLOG(1) << "Failed to encode frame using OpenH264 encoder: error=" << err;
    return;
  }

  if (encoded_callback_.is_null())
    return;

  switch (info.eFrameType) {
    case EVideoFrameType::videoFrameTypeInvalid:
    case EVideoFrameType::videoFrameTypeSkip:
      return;
    default:
      break;
  }

  std::string data;
  data.reserve(CalculateFrameBitStreamLength(info));

  for (int i = 0; i < info.iLayerNum; ++i) {
    const SLayerBSInfo& layer_info = info.sLayerInfo[i];
    data.append(reinterpret_cast<const char*>(layer_info.pBsBuf),
                CalculateLayerBitStreamLength(layer_info));
  }

  const bool key_frame = info.eFrameType == EVideoFrameType::videoFrameTypeIDR;
  encoded_callback_.Run(
      std::unique_ptr<WiFiDisplayEncodedFrame>(new WiFiDisplayEncodedFrame(
          std::move(data), reference_time, base::TimeTicks::Now(), key_frame)));
}

}  // namespace

// static
void WiFiDisplayVideoEncoder::CreateSVC(const InitParameters& params,
                                        VideoEncoderCallback encoder_callback) {
  WiFiDisplayVideoEncoderSVC::Create(params, std::move(encoder_callback));
}

}  // namespace extensions
