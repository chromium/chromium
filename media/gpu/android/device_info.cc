// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/device_info.h"

#include "base/android/build_info.h"
#include "media/base/android/media_codec_util.h"

namespace media {

DeviceInfo* DeviceInfo::GetInstance() {
  static DeviceInfo* info = new DeviceInfo();
  return info;
}

int DeviceInfo::SdkVersion() {
  static const int result = base::android::BuildInfo::GetInstance()->sdk_int();
  return result;
}

bool DeviceInfo::IsVp8DecoderAvailable() {
  static const bool result = MediaCodecUtil::IsVp8DecoderAvailable();
  return result;
}

bool DeviceInfo::IsVp9DecoderAvailable() {
  static const bool result = MediaCodecUtil::IsVp9DecoderAvailable();
  return result;
}

bool DeviceInfo::IsAv1DecoderAvailable() {
  static const bool result = MediaCodecUtil::IsAv1DecoderAvailable();
  return result;
}

bool DeviceInfo::IsDecoderKnownUnaccelerated(VideoCodec codec) {
  return MediaCodecUtil::IsKnownUnaccelerated(codec,
                                              MediaCodecDirection::DECODER);
}

bool DeviceInfo::IsSetOutputSurfaceSupported() {
  static const bool result = MediaCodecUtil::IsSetOutputSurfaceSupported();
  return result;
}

bool DeviceInfo::SupportsOverlaySurfaces() {
  static const bool result = MediaCodecUtil::IsSurfaceViewOutputSupported();
  return result;
}

void DeviceInfo::AddSupportedCodecProfileLevels(
    std::vector<CodecProfileLevel>* result) {
  MediaCodecUtil::AddSupportedCodecProfileLevels(result);
}

}  // namespace media
