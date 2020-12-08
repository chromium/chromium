// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_AUDIO_ENCODER_H_
#define EXTENSIONS_RENDERER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_AUDIO_ENCODER_H_

#include "extensions/renderer/api/display_source/wifi_display/wifi_display_media_encoder.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/wds/src/libwds/public/audio_codec.h"

namespace extensions {

// This interface is a base class for audio used by the Wi-Fi Display media
// pipeline.
// Threading: the client code should belong to a single thread.
class WiFiDisplayAudioEncoder : public WiFiDisplayMediaEncoder,
                                public blink::WebMediaStreamAudioSink {
 public:
  using AudioEncoderCallback =
      base::OnceCallback<void(scoped_refptr<WiFiDisplayAudioEncoder>)>;

  static void Create(const wds::AudioCodec& audio_codec,
                     AudioEncoderCallback encoder_callback);

 protected:
  static const size_t kInvalidCodecModeValue = ~static_cast<size_t>(0u);

  // A factory method that creates a new encoder instance for Linear Pulse-Code
  // Modulation (LPCM) audio encoding.
  static void CreateLPCM(const wds::AudioCodec& audio_codec,
                         AudioEncoderCallback encoder_callback);

  explicit WiFiDisplayAudioEncoder(const wds::AudioCodec& audio_codec);
  ~WiFiDisplayAudioEncoder() override;

  size_t GetAudioCodecMode() const;

  const wds::AudioCodec audio_codec_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_AUDIO_ENCODER_H_
