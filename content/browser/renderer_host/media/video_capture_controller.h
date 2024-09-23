// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_H_

#include <list>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "base/token.h"
#include "base/unguessable_token.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "content/common/content_export.h"
#include "content/public/browser/video_capture_device_launcher.h"
#include "media/capture/mojom/video_capture.mojom.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video/video_frame_receiver.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"
#include "third_party/blink/public/common/media/video_capture.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace content {

class VideoCaptureDeviceLaunchObserver;

// Implementation of media::VideoFrameReceiver that distributes received frames
// to potentially multiple connected clients.
// A call to CreateAndStartDeviceAsync() asynchronously brings up the device. If
// CreateAndStartDeviceAsync() has been called, ReleaseDeviceAsync() must be
// called before releasing the instance.
// Instances must be RefCountedThreadSafe, because an owner
// (VideoCaptureManager) wants to be able to release its reference during an
// (asynchronously executing) run of CreateAndStartDeviceAsync(). To this end,
// the owner passes in the shared ownership as part of |done_cb| into
// CreateAndStartDeviceAsync().
class CONTENT_EXPORT VideoCaptureController
    : public media::VideoFrameReceiver,
      public VideoCaptureDeviceLauncher::Callbacks,
      public base::RefCountedThreadSafe<VideoCaptureController> {
 public:
  VideoCaptureController(
      const std::string& device_id,
      blink::mojom::MediaStreamType stream_type,
      const media::VideoCaptureParams& params,
      std::unique_ptr<VideoCaptureDeviceLauncher> device_launcher,
      base::RepeatingCallback<void(const std::string&)> emit_log_message_cb);

  VideoCaptureController(const VideoCaptureController&) = delete;
  VideoCaptureController& operator=(const VideoCaptureController&) = delete;

  base::WeakPtr<VideoCaptureController> GetWeakPtrForIOThread();

  // Start video capturing and try to use the resolution specified in |params|.
  // Buffers will be shared to the client as necessary. The client will continue
  // to receive frames from the device until RemoveClient() is called.
  void AddClient(const VideoCaptureControllerID& id,
                 VideoCaptureControllerEventHandler* event_handler,
                 const media::VideoCaptureSessionId& session_id,
                 const media::VideoCaptureParams& params,
                 std::optional<url::Origin> origin);

  // Stop video capture. This will take back all buffers held by
  // |event_handler|, and |event_handler| shouldn't use those buffers any more.
  // Returns the session_id of the stopped client, or an empty
  // base::UnguessableToken if the indicated client was not registered.
  base::UnguessableToken RemoveClient(
      const VideoCaptureControllerID& id,
      VideoCaptureControllerEventHandler* event_handler);

  // Pause the video capture for specified client.
  void PauseClient(const VideoCaptureControllerID& id,
                   VideoCaptureControllerEventHandler* event_handler);
  // Resume the video capture for specified client.
  // Returns true if the client will be resumed.
  bool ResumeClient(const VideoCaptureControllerID& id,
                    VideoCaptureControllerEventHandler* event_handler);

  size_t GetClientCount() const;

  // Return true if there is client that isn't paused.
  bool HasActiveClient() const;

  // Return true if there is client paused.
  bool HasPausedClient() const;

  // API called directly by VideoCaptureManager in case the device is
  // prematurely closed.
  void StopSession(const base::UnguessableToken& session_id);

  // Return a buffer with id |buffer_id| previously given in
  // VideoCaptureControllerEventHandler::OnBufferReady.
  // If the consumer provided resource utilization
  // feedback, this will be passed here (-1.0 indicates no feedback).
  void ReturnBuffer(const VideoCaptureControllerID& id,
                    VideoCaptureControllerEventHandler* event_handler,
                    int buffer_id,
                    const media::VideoCaptureFeedback& feedback);

  const std::optional<media::VideoCaptureFormat> GetVideoCaptureFormat() const;

  bool has_received_frames() const { return has_received_frames_; }

  const std::optional<url::Origin> GetFirstClientOrigin() const;

  // Implementation of media::VideoFrameReceiver interface:
  void OnCaptureConfigurationChanged() override;
  void OnNewBuffer(int32_t buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override;
  void OnFrameReadyInBuffer(media::ReadyFrameInBuffer frame) override;
  void OnBufferRetired(int buffer_id) override;
  void OnError(media::VideoCaptureError error) override;
  void OnFrameDropped(media::VideoCaptureFrameDropReason reason) override;
  void OnNewSubCaptureTargetVersion(
      uint32_t sub_capture_target_version) override;
  void OnFrameWithEmptyRegionCapture() override;
  void OnLog(const std::string& message) override;
  void OnStarted() override;
  void OnStartedUsingGpuDecode() override;
  void OnStopped() override;

  // Implementation of VideoCaptureDeviceLauncher::Callbacks interface:
  void OnDeviceLaunched(
      std::unique_ptr<LaunchedVideoCaptureDevice> device) override;
  void OnDeviceLaunchFailed(media::VideoCaptureError error) override;
  void OnDeviceLaunchAborted() override;

  void OnDeviceConnectionLost();

  void CreateAndStartDeviceAsync(
      const media::VideoCaptureParams& params,
      VideoCaptureDeviceLaunchObserver* callbacks,
      base::OnceClosure done_cb,
      mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>
          video_effects_processor);
  void ReleaseDeviceAsync(base::OnceClosure done_cb);
  bool IsDeviceAlive() const;
  void GetPhotoState(
      media::VideoCaptureDevice::GetPhotoStateCallback callback) const;
  void SetPhotoOptions(
      media::mojom::PhotoSettingsPtr settings,
      media::VideoCaptureDevice::SetPhotoOptionsCallback callback);
  void TakePhoto(media::VideoCaptureDevice::TakePhotoCallback callback);
  void MaybeSuspend();
  void Resume();
  void ApplySubCaptureTarget(
      media::mojom::SubCaptureTargetType type,
      const base::Token& target,
      uint32_t sub_capture_target_version,
      base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>
          callback);
  void RequestRefreshFrame();
  void SetDesktopCaptureWindowIdAsync(gfx::NativeViewId window_id,
                                      base::OnceClosure done_cb);
  int serial_id() const { return serial_id_; }
  const std::string& device_id() const { return device_id_; }
  blink::mojom::MediaStreamType stream_type() const { return stream_type_; }
  const media::VideoCaptureParams& parameters() const { return parameters_; }
  bool was_crop_ever_called() const { return was_crop_ever_called_; }

 private:
  friend class base::RefCountedThreadSafe<VideoCaptureController>;
  struct ControllerClient;
  typedef std::list<std::unique_ptr<ControllerClient>> ControllerClients;

  class BufferContext {
   public:
    BufferContext(
        int buffer_context_id,
        int buffer_id,
        media::VideoFrameConsumerFeedbackObserver* consumer_feedback_observer,
        media::mojom::VideoBufferHandlePtr buffer_handle);
    ~BufferContext();
    BufferContext(BufferContext&& other);
    BufferContext& operator=(BufferContext&& other);
    int buffer_context_id() const { return buffer_context_id_; }
    int buffer_id() const { return buffer_id_; }
    bool is_retired() const { return is_retired_; }
    const media::mojom::VideoBufferHandlePtr& buffer_handle() const {
      return buffer_handle_;
    }
    void set_is_retired() { is_retired_ = true; }
    void set_frame_feedback_id(int id) { frame_feedback_id_ = id; }
    void set_consumer_feedback_observer(
        media::VideoFrameConsumerFeedbackObserver* consumer_feedback_observer) {
      consumer_feedback_observer_ = consumer_feedback_observer;
    }
    void set_read_permission(
        std::unique_ptr<
            media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
            buffer_read_permission) {
      buffer_read_permission_ = std::move(buffer_read_permission);
    }
    void RecordConsumerUtilization(const media::VideoCaptureFeedback& feedback);
    void IncreaseConsumerCount();
    void DecreaseConsumerCount();
    bool HasConsumers() const { return consumer_hold_count_ > 0; }
    media::mojom::VideoBufferHandlePtr CloneBufferHandle();

   private:
    int buffer_context_id_;
    int buffer_id_;
    bool is_retired_;
    int frame_feedback_id_;
    raw_ptr<media::VideoFrameConsumerFeedbackObserver>
        consumer_feedback_observer_;
    media::mojom::VideoBufferHandlePtr buffer_handle_;
    media::VideoCaptureFeedback combined_consumer_feedback_;

    int consumer_hold_count_;
    std::unique_ptr<
        media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
        buffer_read_permission_;
  };

  ~VideoCaptureController() override;

  // Find a client of |id| and |handler| in |clients|.
  ControllerClient* FindClient(const VideoCaptureControllerID& id,
                               VideoCaptureControllerEventHandler* handler,
                               const ControllerClients& clients);

  // Find a client of |session_id| in |clients|.
  ControllerClient* FindClient(const base::UnguessableToken& session_id,
                               const ControllerClients& clients);

  std::vector<BufferContext>::iterator FindBufferContextFromBufferContextId(
      int buffer_context_id);
  std::vector<BufferContext>::iterator FindUnretiredBufferContextFromBufferId(
      int buffer_id);

  ReadyBuffer MakeReadyBufferAndSetContextFeedbackId(
      int buffer_id,
      int frame_feedback_id,
      media::mojom::VideoFrameInfoPtr frame_info,
      BufferContext** out_buffer_context);
  void MakeClientUseBufferContext(BufferContext* frame_context,
                                  ControllerClient* client);

  void OnClientFinishedConsumingBuffer(
      ControllerClient* client,
      int buffer_id,
      const media::VideoCaptureFeedback& feedback);
  void ReleaseBufferContext(
      const std::vector<BufferContext>::iterator& buffer_state_iter);

  using EventHandlerAction =
      base::RepeatingCallback<void(VideoCaptureControllerEventHandler* client,
                                   const VideoCaptureControllerID& id)>;
  void PerformForClientsWithOpenSession(EventHandlerAction action);

  void EmitLogMessage(const std::string& message, int verbose_log_level);

  void MaybeEmitFrameDropLogMessage(media::VideoCaptureFrameDropReason reason);

  const int serial_id_;
  const std::string device_id_;
  const blink::mojom::MediaStreamType stream_type_;
  const media::VideoCaptureParams parameters_;
  std::unique_ptr<VideoCaptureDeviceLauncher> device_launcher_;
  base::RepeatingCallback<void(const std::string&)> emit_log_message_cb_;
  std::unique_ptr<LaunchedVideoCaptureDevice> launched_device_;
  raw_ptr<VideoCaptureDeviceLaunchObserver> device_launch_observer_;

  std::vector<BufferContext> buffer_contexts_;

  // All clients served by this controller.
  ControllerClients controller_clients_;

  // Takes on only the states 'STARTING', 'STARTED' and 'ERROR'. 'ERROR' is an
  // absorbing state which stops the flow of data to clients.
  blink::VideoCaptureState state_;

  int next_buffer_context_id_ = 0;

  // True if the controller has received a video frame from the device.
  bool has_received_frames_;
  base::TimeTicks time_of_start_request_;

  std::optional<media::VideoCaptureFormat> video_capture_format_;

  std::optional<url::Origin> first_client_origin_;

  // As a work-around to technical limitations, we don't allow multiple
  // captures of the same tab, by the same capturer, if the first capturer
  // invoked cropping. (Any capturer but the first one would have been
  // blocked earlier in the pipeline.) That is because the
  // `sub_capture_target_version` would otherwise not line up between the
  // various ControllerClients.
  bool was_crop_ever_called_ = false;

  base::WeakPtrFactory<VideoCaptureController> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_H_
