// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/webrtc_util.h"

#include <cstring>

#include "third_party/blink/renderer/platform/network/parsed_content_type.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"

namespace blink {

String WebrtcCodecNameFromMimeType(const String& mime_type,
                                   const char* prefix) {
  if (mime_type.StartsWith(prefix)) {
    wtf_size_t length =
        static_cast<wtf_size_t>(mime_type.length() - strlen(prefix) - 1);
    const String codec_name = mime_type.Right(length);
    return codec_name;
  }
  return "";
}

webrtc::SdpVideoFormat::Parameters ConvertToSdpVideoFormatParameters(
    const ParsedContentHeaderFieldParameters& parameters) {
  webrtc::SdpVideoFormat::Parameters sdp_parameters;
  for (const auto& parameter : parameters) {
    sdp_parameters[parameter.name.Utf8()] = parameter.value.Utf8();
  }
  return sdp_parameters;
}

base::TimeTicks PLATFORM_EXPORT ConvertToBaseTimeTicks(webrtc::Timestamp time) {
  if (time == webrtc::Timestamp::PlusInfinity()) {
    return base::TimeTicks::Max();
  } else if (time == webrtc::Timestamp::MinusInfinity()) {
    return base::TimeTicks::Min();
  } else {
    return base::TimeTicks() + base::Microseconds(time.us());
  }
}

}  // namespace blink
