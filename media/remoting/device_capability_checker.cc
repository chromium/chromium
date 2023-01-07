// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/device_capability_checker.h"

#include "base/strings/string_util.h"
#include "media/base/audio_codecs.h"
#include "media/base/video_codecs.h"

namespace media::remoting {

bool IsChromecast(const std::string& model_name) {
  // This is a workaround for Nest Hub devices, which do not support remoting.
  // TODO(crbug.com/1198616): filtering hack should be removed. See b/135725157
  // for more information.
  return base::StartsWith(model_name, "Chromecast",
                          base::CompareCase::SENSITIVE) ||
         base::StartsWith(model_name, "Eureka Dongle",
                          base::CompareCase::SENSITIVE);
}

bool IsVideoCodecCompatible(const std::string& model_name,
                            VideoCodec video_codec) {
  if (!IsChromecast(model_name)) {
    return false;
  }

  if (video_codec == VideoCodec::kH264 || video_codec == VideoCodec::kVP8) {
    return true;
  }
  if (model_name == "Chromecast Ultra" &&
      (video_codec == VideoCodec::kHEVC || video_codec == VideoCodec::kVP9)) {
    return true;
  }
  return false;
}

bool IsAudioCodecCompatible(const std::string& model_name,
                            AudioCodec audio_codec) {
  if (!IsChromecast(model_name)) {
    return false;
  }
  return (audio_codec == AudioCodec::kAAC) ||
         (audio_codec == AudioCodec::kOpus);
}
}  // namespace media::remoting
