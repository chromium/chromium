// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_DISPATCHER_HOST_H_

#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/media/media_devices_util.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/mediastream/media_stream_controls.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace content {

class MediaStreamManager;

// MediaStreamDispatcherHost is a delegate for Media Stream API messages used by
// MediaStreamImpl.  There is one MediaStreamDispatcherHost per
// RenderProcessHost, the former owned by the latter.
class CONTENT_EXPORT MediaStreamDispatcherHost
    : public blink::mojom::MediaStreamDispatcherHost {
 public:
  MediaStreamDispatcherHost(int render_process_id,
                            int render_frame_id,
                            MediaStreamManager* media_stream_manager);
  ~MediaStreamDispatcherHost() override;
  static void Create(
      int render_process_id,
      int render_frame_id,
      MediaStreamManager* media_stream_manager,
      mojo::PendingReceiver<blink::mojom::MediaStreamDispatcherHost> receiver);

  void set_salt_and_origin_callback_for_testing(
      MediaDeviceSaltAndOriginCallback callback) {
    salt_and_origin_callback_ = std::move(callback);
  }
  void SetMediaStreamDeviceObserverForTesting(
      mojo::PendingRemote<blink::mojom::MediaStreamDeviceObserver> observer) {
    media_stream_device_observer_.Bind(std::move(observer));
  }

 private:
  friend class MockMediaStreamDispatcherHost;

  const mojo::Remote<blink::mojom::MediaStreamDeviceObserver>&
  GetMediaStreamDeviceObserver();
  void OnMediaStreamDeviceObserverConnectionError();
  void CancelAllRequests();

  // mojom::MediaStreamDispatcherHost implementation
  void GenerateStream(
      int32_t request_id,
      const blink::StreamControls& controls,
      bool user_gesture,
      blink::mojom::StreamSelectionInfoPtr audio_stream_selection_info_ptr,
      GenerateStreamCallback callback) override;
  void CancelRequest(int32_t request_id) override;
  void StopStreamDevice(
      const std::string& device_id,
      const base::Optional<base::UnguessableToken>& session_id) override;
  void OpenDevice(int32_t request_id,
                  const std::string& device_id,
                  blink::mojom::MediaStreamType type,
                  OpenDeviceCallback callback) override;
  void CloseDevice(const std::string& label) override;
  void SetCapturingLinkSecured(
      const base::Optional<base::UnguessableToken>& session_id,
      blink::mojom::MediaStreamType type,
      bool is_secure) override;
  void OnStreamStarted(const std::string& label) override;

  void DoGenerateStream(
      int32_t request_id,
      const blink::StreamControls& controls,
      bool user_gesture,
      blink::mojom::StreamSelectionInfoPtr audio_stream_selection_info_ptr,
      GenerateStreamCallback callback,
      MediaDeviceSaltAndOrigin salt_and_origin);
  void DoOpenDevice(int32_t request_id,
                    const std::string& device_id,
                    blink::mojom::MediaStreamType type,
                    OpenDeviceCallback callback,
                    MediaDeviceSaltAndOrigin salt_and_origin);

  void OnDeviceStopped(const std::string& label,
                       const blink::MediaStreamDevice& device);
  void OnDeviceChanged(const std::string& label,
                       const blink::MediaStreamDevice& old_device,
                       const blink::MediaStreamDevice& new_device);

  static int next_requester_id_;

  const int render_process_id_;
  const int render_frame_id_;
  const int requester_id_;
  MediaStreamManager* media_stream_manager_;
  mojo::Remote<blink::mojom::MediaStreamDeviceObserver>
      media_stream_device_observer_;
  MediaDeviceSaltAndOriginCallback salt_and_origin_callback_;

  base::WeakPtrFactory<MediaStreamDispatcherHost> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_DISPATCHER_HOST_H_
