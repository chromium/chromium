// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_MEDIA_MANAGER_H_
#define EXTENSIONS_RENDERER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_MEDIA_MANAGER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "extensions/common/mojom/wifi_display_session_service.mojom.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_media_packetizer.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_video_encoder.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/wds/src/libwds/public/media_manager.h"

namespace service_manager {
class InterfaceProvider;
}

namespace extensions {
class WiFiDisplayAudioSink;
class WiFiDisplayVideoSink;
class WiFiDisplayMediaPipeline;

class WiFiDisplayMediaManager : public wds::SourceMediaManager {
 public:
  using ErrorCallback = base::Callback<void(const std::string&)>;

  WiFiDisplayMediaManager(
      const blink::WebMediaStreamTrack& video_track,
      const blink::WebMediaStreamTrack& audio_track,
      const net::IPAddress& sink_ip_address,
      service_manager::InterfaceProvider* interface_provider,
      const ErrorCallback& error_callback);

  ~WiFiDisplayMediaManager() override;

 private:
  // wds::SourceMediaManager overrides.
  void Play() override;

  void Pause() override;
  void Teardown() override;
  bool IsPaused() const override;
  wds::SessionType GetSessionType() const override;
  void SetSinkRtpPorts(int port1, int port2) override;
  std::pair<int, int> GetSinkRtpPorts() const override;
  int GetLocalRtpPort() const override;

  bool InitOptimalVideoFormat(
      const wds::NativeVideoFormat& sink_native_format,
      const std::vector<wds::H264VideoCodec>& sink_supported_codecs) override;
  wds::H264VideoFormat GetOptimalVideoFormat() const override;
  bool InitOptimalAudioFormat(
      const std::vector<wds::AudioCodec>& sink_supported_codecs) override;
  wds::AudioCodec GetOptimalAudioFormat() const override;

  void SendIDRPicture() override;

  std::string GetSessionId() const override;

 private:
  void OnPlayerCreated(std::unique_ptr<WiFiDisplayMediaPipeline> player);
  void OnMediaPipelineInitialized(bool success);
  void RegisterMediaService(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_runner,
      mojo::PendingReceiver<mojom::WiFiDisplayMediaService> service,
      const base::Closure& on_completed);
  void ConnectToRemoteService(
      mojo::PendingReceiver<mojom::WiFiDisplayMediaService> receiver);
  blink::WebMediaStreamTrack video_track_;
  blink::WebMediaStreamTrack audio_track_;

  std::unique_ptr<WiFiDisplayAudioSink> audio_sink_;
  std::unique_ptr<WiFiDisplayVideoSink> video_sink_;

  service_manager::InterfaceProvider* interface_provider_;
  net::IPAddress sink_ip_address_;
  std::pair<int, int> sink_rtp_ports_;
  wds::H264VideoFormat optimal_video_format_;
  wds::AudioCodec optimal_audio_codec_;

  WiFiDisplayVideoEncoder::InitParameters video_encoder_parameters_;
  WiFiDisplayMediaPipeline* player_;  // Owned on IO thread.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  ErrorCallback error_callback_;
  bool is_playing_;
  bool is_initialized_;
  mutable std::string session_id_;  // Lazily initialized.

  base::WeakPtrFactory<WiFiDisplayMediaManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WiFiDisplayMediaManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_MEDIA_MANAGER_H_
