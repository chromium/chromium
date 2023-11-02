// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_DEVICE_CAPABILITY_CHECKER_H_
#define MEDIA_REMOTING_DEVICE_CAPABILITY_CHECKER_H_

#include <string>

namespace media {
enum class AudioCodec;
enum class VideoCodec;

namespace remoting {
// Return true if the device is a Chromecast device according to its
// `model_name`.
bool IsChromecast(const std::string& model_name);

// Return true if the device is compatible to render `video_codec`.
bool IsVideoCodecCompatible(const std::string& model_name,
                            media::VideoCodec video_codec);

// Return true if the device is compatible to render `audio_codec`.
bool IsAudioCodecCompatible(const std::string& model_name,
                            media::AudioCodec audio_codec);
}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_DEVICE_CAPABILITY_CHECKER_H_
