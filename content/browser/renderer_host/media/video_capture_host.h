// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_HOST_H_

#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/token.h"
#include "base/unguessable_token.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_routing_id.h"
#include "media/capture/mojom/video_capture.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class MediaStreamManager;

// VideoCaptureHost is the IO thread browser process communication endpoint
// between a render frame (which can initiate and receive a video capture
// stream) and a VideoCaptureController in the browser process (which provides
// the stream from a video device). Every remote client is identified via a
// unique |device_id|, and is paired with a single VideoCaptureController.
class CONTENT_EXPORT VideoCaptureHost
    : public VideoCaptureControllerEventHandler,
      public media::mojom::VideoCaptureHost {
 public:
  VideoCaptureHost(GlobalRenderFrameHostId render_frame_host_id,
                   MediaStreamManager* media_stream_manager);
  class RenderFrameHostDelegate;
  VideoCaptureHost(std::unique_ptr<RenderFrameHostDelegate> delegate,
                   MediaStreamManager* media_stream_manager);

  VideoCaptureHost(const VideoCaptureHost&) = delete;
  VideoCaptureHost& operator=(const VideoCaptureHost&) = delete;

  ~VideoCaptureHost() override;

  static void Create(
      GlobalRenderFrameHostId render_frame_host_id,
      MediaStreamManager* media_stream_manager,
      mojo::PendingReceiver<media::mojom::VideoCaptureHost> receiver);

  // Interface for notifying RenderFrameHost instance about active video
  // capture stream changes and getting its ID.
  class CONTENT_EXPORT RenderFrameHostDelegate {
   public:
    virtual ~RenderFrameHostDelegate();
    virtual void NotifyStreamAdded() = 0;
    virtual void NotifyStreamRemoved() = 0;
    virtual GlobalRenderFrameHostId GetRenderFrameHostId() const = 0;
  };

 private:
  friend class VideoCaptureTest;
  FRIEND_TEST_ALL_PREFIXES(VideoCaptureTest, IncrementMatchesDecrementCalls);

  // VideoCaptureControllerEventHandler implementation.
  void OnCaptureConfigurationChanged(
      const VideoCaptureControllerID& id) override;
  void OnError(const VideoCaptureControllerID& id,
               media::VideoCaptureError error) override;
  void OnNewBuffer(const VideoCaptureControllerID& id,
                   media::mojom::VideoBufferHandlePtr buffer_handle,
                   int buffer_id) override;
  void OnBufferDestroyed(const VideoCaptureControllerID& id,
                         int buffer_id) override;
  void OnBufferReady(const VideoCaptureControllerID& controller_id,
                     const ReadyBuffer& buffer) override;
  void OnFrameDropped(const VideoCaptureControllerID& controller_id,
                      media::VideoCaptureFrameDropReason reason) override;
  void OnFrameWithEmptyRegionCapture(
      const VideoCaptureControllerID& controller_id) override;
  void OnEnded(const VideoCaptureControllerID& id) override;
  void OnStarted(const VideoCaptureControllerID& id) override;
  void OnStartedUsingGpuDecode(const VideoCaptureControllerID& id) override;

  // media::mojom::VideoCaptureHost implementation
  void Start(const base::UnguessableToken& device_id,
             const base::UnguessableToken& session_id,
             const media::VideoCaptureParams& params,
             mojo::PendingRemote<media::mojom::VideoCaptureObserver> observer)
      override;
  void Stop(const base::UnguessableToken& device_id) override;
  void Pause(const base::UnguessableToken& device_id) override;
  void Resume(const base::UnguessableToken& device_id,
              const base::UnguessableToken& session_id,
              const media::VideoCaptureParams& params) override;
  void RequestRefreshFrame(const base::UnguessableToken& device_id) override;
  void ReleaseBuffer(const base::UnguessableToken& device_id,
                     int32_t buffer_id,
                     const media::VideoCaptureFeedback& feedback) override;
  void GetDeviceSupportedFormats(
      const base::UnguessableToken& device_id,
      const base::UnguessableToken& session_id,
      GetDeviceSupportedFormatsCallback callback) override;
  void GetDeviceFormatsInUse(const base::UnguessableToken& device_id,
                             const base::UnguessableToken& session_id,
                             GetDeviceFormatsInUseCallback callback) override;
  // This refers to a late frame drop, originating from the renderer process.
  void OnNewSubCaptureTargetVersion(
      const base::UnguessableToken& device_id,
      uint32_t sub_capture_target_version) override;
  void OnLog(const base::UnguessableToken& device_id,
             const std::string& message) override;

  void DoError(const VideoCaptureControllerID& id,
               media::VideoCaptureError error);
  void DoEnded(const VideoCaptureControllerID& id);

  // Bound as callback for VideoCaptureManager::StartCaptureForClient().
  void OnControllerAdded(
      const base::UnguessableToken& device_id,
      const base::WeakPtr<VideoCaptureController>& controller);

  // Helper function that deletes the controller and tells VideoCaptureManager
  // to StopCaptureForClient(). |on_error| is true if this is triggered by
  // VideoCaptureControllerEventHandler::OnError.
  void DeleteVideoCaptureController(
      const VideoCaptureControllerID& controller_id,
      media::VideoCaptureError error);

  void NotifyStreamAdded();
  void NotifyStreamRemoved();
  void NotifyAllStreamsRemoved();

  void ConnectClient(const base::UnguessableToken session_id,
                     const media::VideoCaptureParams& params,
                     VideoCaptureControllerID controller_id,
                     VideoCaptureManager::DoneCB done_cb,
                     BrowserContext* browser_context);

  class RenderFrameHostDelegateImpl;
  std::unique_ptr<RenderFrameHostDelegate> render_frame_host_delegate_;
  uint32_t number_of_active_streams_ = 0;

  const raw_ptr<MediaStreamManager> media_stream_manager_;

  // A map of VideoCaptureControllerID to the VideoCaptureController to which it
  // is connected. An entry in this map holds a null controller while it is in
  // the process of starting.
  std::map<VideoCaptureControllerID, base::WeakPtr<VideoCaptureController>>
      controllers_;

  // VideoCaptureObservers map, each one is used and should be valid between
  // Start() and the corresponding Stop().
  std::map<base::UnguessableToken,
           mojo::Remote<media::mojom::VideoCaptureObserver>>
      device_id_to_observer_map_;

  std::optional<gfx::Rect> region_capture_rect_;

  base::WeakPtrFactory<VideoCaptureHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_HOST_H_
