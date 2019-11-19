// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_MEDIA_PIPELINE_H_
#define EXTENSIONS_RENDERER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_MEDIA_PIPELINE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "extensions/common/mojom/wifi_display_session_service.mojom.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_audio_encoder.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_media_packetizer.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_video_encoder.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/wds/src/libwds/public/media_manager.h"

namespace extensions {

// This class encapsulates the WiFi Display media pipeline including
// - encoding
// - AV multiplexing/packetization
// - sending
// Threading: should belong to IO thread.
class WiFiDisplayMediaPipeline {
 public:
  using ErrorCallback = base::Callback<void(const std::string&)>;
  using InitCompletionCallback = base::Callback<void(bool)>;
  using RegisterMediaServiceCallback =
      base::Callback<void(mojo::PendingReceiver<mojom::WiFiDisplayMediaService>,
                          const base::Closure&)>;

  static std::unique_ptr<WiFiDisplayMediaPipeline> Create(
      wds::SessionType type,
      const WiFiDisplayVideoEncoder::InitParameters& video_parameters,
      const wds::AudioCodec& audio_codec,
      const net::IPAddress& sink_ip_address,
      const std::pair<int, int>& sink_rtp_ports,
      const RegisterMediaServiceCallback& service_callback,
      const ErrorCallback& error_callback);
  ~WiFiDisplayMediaPipeline();
  // Note: to be called only once.
  void Initialize(const InitCompletionCallback& callback);

  void InsertRawVideoFrame(scoped_refptr<media::VideoFrame> video_frame,
                           base::TimeTicks reference_time);

  void RequestIDRPicture();

  WiFiDisplayAudioEncoder* audio_sink() { return audio_encoder_.get(); }

 private:
  using InitStepCompletionCallback = InitCompletionCallback;
  enum class InitializationStep : unsigned;

  WiFiDisplayMediaPipeline(
      wds::SessionType type,
      const WiFiDisplayVideoEncoder::InitParameters& video_parameters,
      const wds::AudioCodec& audio_codec,
      const net::IPAddress& sink_ip_address,
      const std::pair<int, int>& sink_rtp_ports,
      const RegisterMediaServiceCallback& service_callback,
      const ErrorCallback& error_callback);

  void CreateMediaPacketizer();
  void OnInitialize(const InitCompletionCallback& callback,
                    InitializationStep current_step,
                    bool success);
  void OnAudioEncoderCreated(
      const InitStepCompletionCallback& callback,
      scoped_refptr<WiFiDisplayAudioEncoder> audio_encoder);
  void OnVideoEncoderCreated(
      const InitStepCompletionCallback& callback,
      scoped_refptr<WiFiDisplayVideoEncoder> video_encoder);
  void OnMediaServiceRegistered(const InitCompletionCallback& callback);

  void OnEncodedAudioUnit(std::unique_ptr<WiFiDisplayEncodedUnit> unit);
  void OnEncodedVideoFrame(std::unique_ptr<WiFiDisplayEncodedFrame> frame);

  bool OnPacketizedMediaDatagramPacket(
     WiFiDisplayMediaDatagramPacket media_datagram_packet);

  scoped_refptr<WiFiDisplayAudioEncoder> audio_encoder_;
  scoped_refptr<WiFiDisplayVideoEncoder> video_encoder_;
  std::unique_ptr<WiFiDisplayMediaPacketizer> packetizer_;

  wds::SessionType type_;
  WiFiDisplayVideoEncoder::InitParameters video_parameters_;
  wds::AudioCodec audio_codec_;
  net::IPAddress sink_ip_address_;
  std::pair<int, int> sink_rtp_ports_;

  RegisterMediaServiceCallback service_callback_;
  ErrorCallback error_callback_;
  mojo::Remote<mojom::WiFiDisplayMediaService> media_service_;

  base::WeakPtrFactory<WiFiDisplayMediaPipeline> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WiFiDisplayMediaPipeline);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_MEDIA_PIPELINE_H_
