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
// Return true if the Cast receiver device is known to support media remoting
// according to its `model_name`.  Note that this does not include all Cast
// receivers that support remoting.
bool IsKnownToSupportRemoting(const std::string& model_name);

// Return true if the Cast receiver with `model_name` is compatible to render
// `video_codec`.
bool IsVideoCodecCompatible(const std::string& model_name,
                            media::VideoCodec video_codec);

// Return true if the Cast receiver with `model_name` is compatible to render
// `audio_codec`.
bool IsAudioCodecCompatible(const std::string& model_name,
                            media::AudioCodec audio_codec);

// Custom codec parsing function for media remoting.
media::VideoCodec ParseVideoCodec(const std::string& codec_str);
media::AudioCodec ParseAudioCodec(const std::string& codec_str);

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_DEVICE_CAPABILITY_CHECKER_H_
