// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/transmission_encoding_info_handler.h"

#include <utility>
#include <vector>

#include "base/cpu.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/media_capabilities/web_media_configuration.h"
#include "third_party/blink/renderer/platform/media_capabilities/web_video_configuration.h"
#include "third_party/blink/renderer/platform/peerconnection/audio_codec_factory.h"
#include "third_party/blink/renderer/platform/peerconnection/video_codec_factory.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/webrtc/api/audio_codecs/audio_encoder_factory.h"
#include "third_party/webrtc/api/audio_codecs/audio_format.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/api/video_codecs/video_encoder_factory.h"

namespace blink {

namespace {

// Composes elements of set<string> to a string with ", " delimiter.
String StringHashSetToString(const HashSet<String>& string_set) {
  String result;
  String delim;
  for (auto& s : string_set) {
    result = result + delim + s;
    if (delim.IsEmpty())
      delim = ", ";
  }
  return result;
}

// Composes human readable string for |info|.
String ToString(const blink::WebMediaCapabilitiesInfo& info) {
  return String::Format("(supported:%s, smooth:%s, power_efficient:%s)",
                        info.supported ? "true" : "false",
                        info.smooth ? "true" : "false",
                        info.power_efficient ? "true" : "false");
}

// Gets GpuVideoAcceleratorFactories instance pointer.
// Returns nullptr if RenderThreadImpl instance is not available.
media::GpuVideoAcceleratorFactories* GetGpuFactories() {
  return Platform::Current()->GetGpuFactories();
}

// Returns true if CPU can encode HD video smoothly.
// The logic is borrowed from Google Meet (crbug.com/941352).
bool CanCpuEncodeHdSmoothly() {
  const int num_processors = base::SysInfo::NumberOfProcessors();
  if (num_processors >= 4)
    return true;
  if (num_processors < 2)
    return false;
  return base::CPU().has_sse41();
}

const unsigned int kHdVideoAreaSize = 1280 * 720;

}  // namespace

TransmissionEncodingInfoHandler* TransmissionEncodingInfoHandler::Instance() {
  DEFINE_STATIC_LOCAL(TransmissionEncodingInfoHandler, instance, ());
  return &instance;
}

// If GetGpuFactories() returns null, CreateWebrtcVideoEncoderFactory()
// returns software encoder factory only.
TransmissionEncodingInfoHandler::TransmissionEncodingInfoHandler()
    : TransmissionEncodingInfoHandler(
          blink::CreateWebrtcVideoEncoderFactory(GetGpuFactories()),
          CanCpuEncodeHdSmoothly()) {}

TransmissionEncodingInfoHandler::TransmissionEncodingInfoHandler(
    std::unique_ptr<webrtc::VideoEncoderFactory> video_encoder_factory,
    bool cpu_hd_smooth)
    : cpu_hd_smooth_(cpu_hd_smooth) {
  std::vector<webrtc::SdpVideoFormat> supported_video_formats =
      video_encoder_factory->GetSupportedFormats();
  for (const auto& video_format : supported_video_formats) {
    const String codec_name = String::FromUTF8(video_format.name).LowerASCII();
    supported_video_codecs_.insert(codec_name);
    const auto codec_info =
        video_encoder_factory->QueryVideoEncoder(video_format);
    if (codec_info.is_hardware_accelerated)
      hardware_accelerated_video_codecs_.insert(codec_name);
  }

  rtc::scoped_refptr<webrtc::AudioEncoderFactory> audio_encoder_factory =
      blink::CreateWebrtcAudioEncoderFactory();
  std::vector<webrtc::AudioCodecSpec> supported_audio_specs =
      audio_encoder_factory->GetSupportedEncoders();
  for (const auto& audio_spec : supported_audio_specs) {
    supported_audio_codecs_.insert(
        String::FromUTF8(audio_spec.format.name).LowerASCII());
  }
  DVLOG(2) << String::Format(
      "supported_video_codecs_:[%s] hardware_accelerated_video_codecs_:[%s] "
      "supported_audio_codecs_:[%s]",
      StringHashSetToString(supported_video_codecs_).Utf8().c_str(),
      StringHashSetToString(hardware_accelerated_video_codecs_).Utf8().c_str(),
      StringHashSetToString(supported_audio_codecs_).Utf8().c_str());
}

TransmissionEncodingInfoHandler::~TransmissionEncodingInfoHandler() = default;

String TransmissionEncodingInfoHandler::ExtractSupportedCodecFromMimeType(
    const String& mime_type) const {
  const char* video_prefix = "video/";
  const char* audio_prefix = "audio/";
  if (mime_type.StartsWith(video_prefix)) {
    // Currently support "video/vp8" only.
    // TODO(crbug.com/941320): support "video/vp9" and "video/h264" once their
    // MIME type parser are implemented.
    const String codec_name = mime_type.Substring(strlen(video_prefix));
    if (codec_name == "vp8")
      return codec_name;
  } else if (mime_type.StartsWith(audio_prefix)) {
    const String codec_name = mime_type.Substring(strlen(audio_prefix));
    if (base::Contains(supported_audio_codecs_, codec_name))
      return codec_name;
  }
  return "";
}

bool TransmissionEncodingInfoHandler::CanCpuEncodeSmoothly(
    const blink::WebVideoConfiguration& configuration) const {
  if (configuration.width * configuration.height < kHdVideoAreaSize)
    return true;
  return cpu_hd_smooth_;
}

void TransmissionEncodingInfoHandler::EncodingInfo(
    const blink::WebMediaConfiguration& configuration,
    OnMediaCapabilitiesEncodingInfoCallback callback) const {
  DCHECK(configuration.video_configuration ||
         configuration.audio_configuration);

  auto info = std::make_unique<blink::WebMediaCapabilitiesInfo>();
  if (!configuration.video_configuration &&
      !configuration.audio_configuration) {
    DVLOG(2) << "Neither video nor audio configuration specified.";
    std::move(callback).Run(std::move(info));
    return;
  }

  // Either video or audio capabilities will be AND-ed so set |info|'s default
  // value to true.
  info->supported = info->smooth = info->power_efficient = true;
  if (configuration.video_configuration) {
    const auto& video_config = configuration.video_configuration.value();
    const String mime_type = String(video_config.mime_type).LowerASCII();
    const String codec_name = ExtractSupportedCodecFromMimeType(mime_type);
    info->supported = !codec_name.IsEmpty();
    if (info->supported) {
      const bool is_hardware_accelerated =
          base::Contains(hardware_accelerated_video_codecs_, codec_name);
      info->smooth =
          is_hardware_accelerated || CanCpuEncodeSmoothly(video_config);
      info->power_efficient = is_hardware_accelerated;
    } else {
      info->smooth = false;
      info->power_efficient = false;
    }
    DVLOG(2) << "Video MIME type:" << mime_type
             << " capabilities:" << ToString(*info);
  }
  if (configuration.audio_configuration) {
    const String mime_type =
        String(configuration.audio_configuration->mime_type).LowerASCII();
    const String codec_name = ExtractSupportedCodecFromMimeType(mime_type);
    info->supported &= !codec_name.IsEmpty();
    // Audio is always assumed to be smooth and efficient whenever it is
    // supported.
    info->smooth &= info->supported;
    info->power_efficient &= info->supported;
    DVLOG(2) << "Audio MIME type:" << mime_type
             << " capabilities:" << ToString(*info);
  }
  std::move(callback).Run(std::move(info));
}

}  // namespace blink
