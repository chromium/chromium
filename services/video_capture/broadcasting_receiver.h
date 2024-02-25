// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_BROADCASTING_RECEIVER_H_
#define SERVICES_VIDEO_CAPTURE_BROADCASTING_RECEIVER_H_

#include <map>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "media/capture/video/video_frame_receiver.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/cpp/video_frame_access_handler.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"

namespace video_capture {

// Implementation of media::VideoFrameReceiver that distributes frames to
// potentially multiple clients.
class BroadcastingReceiver : public media::VideoFrameReceiver {
 public:
  class BufferContext {
   public:
    BufferContext(int32_t buffer_id,
                  media::mojom::VideoBufferHandlePtr buffer_handle);
    ~BufferContext();
    BufferContext(BufferContext&& other);
    BufferContext& operator=(BufferContext&& other);
    int32_t buffer_context_id() const { return buffer_context_id_; }
    int32_t buffer_id() const { return buffer_id_; }
    void IncreaseConsumerCount();
    void DecreaseConsumerCount();
    bool IsStillBeingConsumed() const;
    bool is_retired() const { return is_retired_; }
    void set_retired() { is_retired_ = true; }
    media::mojom::VideoBufferHandlePtr CloneBufferHandle(
        media::VideoCaptureBufferType target_buffer_type);

   private:
    int32_t buffer_context_id_;
    int32_t buffer_id_;
    media::mojom::VideoBufferHandlePtr buffer_handle_;
    // Indicates how many consumers are currently relying on
    // |access_permission_|.
    int32_t consumer_hold_count_;
    bool is_retired_;
  };

  BroadcastingReceiver();
  ~BroadcastingReceiver() override;

  base::WeakPtr<media::VideoFrameReceiver> GetWeakPtr();

  // Indicates to the BroadcastingReceiver that we want to restart the source
  // without letting connected clients know about the restart. The
  // BroadcastingReceiver will hide the OnStopped() event sent by the source
  // from the connected clients and instead invoke the given
  // |on_stopped_handler|. It will also not forward the subsequent
  // OnStarted() and possibly OnStartedUsingGpuDecode() events to clients who
  // have already received these events.
  void HideSourceRestartFromClients(base::OnceClosure on_stopped_handler);

  void SetOnStoppedHandler(base::OnceClosure on_stopped_handler);

  // Returns a client_id that can be used for a call to Suspend/Resume/Remove.
  int32_t AddClient(mojo::PendingRemote<mojom::VideoFrameHandler> client,
                    media::VideoCaptureBufferType target_buffer_type);
  void SuspendClient(int32_t client_id);
  void ResumeClient(int32_t client_id);
  // Returns ownership of the client back to the caller.
  mojo::Remote<mojom::VideoFrameHandler> RemoveClient(int32_t client_id);

  // media::VideoFrameReceiver:
  void OnCaptureConfigurationChanged() override;
  void OnNewBuffer(int32_t buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override;
  void OnFrameReadyInBuffer(media::ReadyFrameInBuffer frame) override;
  void OnBufferRetired(int32_t buffer_id) override;
  void OnError(media::VideoCaptureError error) override;
  void OnFrameDropped(media::VideoCaptureFrameDropReason reason) override;
  void OnNewSubCaptureTargetVersion(
      uint32_t sub_capture_target_version) override;
  void OnFrameWithEmptyRegionCapture() override;
  void OnLog(const std::string& message) override;
  void OnStarted() override;
  void OnStartedUsingGpuDecode() override;
  void OnStopped() override;

 private:
  friend class BroadcastingReceiverTest;

  enum class Status {
    kOnStartedHasNotYetBeenCalled,
    kOnStartedHasBeenCalled,
    kOnStartedUsingGpuDecodeHasBeenCalled,
    kDeviceIsRestarting,
    kOnErrorHasBeenCalled,
    kOnStoppedHasBeenCalled
  };

  // Wrapper that suppresses calls to OnStarted() and OnStartedUsingGpuDecode()
  // after they have already been called once. Keeps track of whether or not
  // a client is suspended.
  class ClientContext {
   public:
    ClientContext(mojo::PendingRemote<mojom::VideoFrameHandler> client,
                  media::VideoCaptureBufferType target_buffer_type);
    ~ClientContext();
    ClientContext(ClientContext&& other);
    ClientContext& operator=(ClientContext&& other);
    void OnStarted();
    void OnStartedUsingGpuDecode();

    mojo::Remote<mojom::VideoFrameHandler>& client() { return client_; }
    media::VideoCaptureBufferType target_buffer_type() {
      return target_buffer_type_;
    }
    void set_is_suspended(bool suspended) { is_suspended_ = suspended; }
    bool is_suspended() const { return is_suspended_; }
    void set_has_client_frame_access_handler_remote() {
      has_client_frame_access_handler_remote_ = true;
    }
    bool has_client_frame_access_handler_remote() const {
      return has_client_frame_access_handler_remote_;
    }

   private:
    mojo::Remote<mojom::VideoFrameHandler> client_;
    media::VideoCaptureBufferType target_buffer_type_;
    bool is_suspended_;
    bool on_started_has_been_called_;
    bool on_started_using_gpu_decode_has_been_called_;
    bool has_client_frame_access_handler_remote_;
  };

  class ClientVideoFrameAccessHandler;

  void OnClientFinishedConsumingFrame(int32_t buffer_context_id);
  void OnClientDisconnected(int32_t client_id);
  std::vector<BufferContext>::iterator FindUnretiredBufferContextFromBufferId(
      int32_t buffer_id);

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<VideoFrameAccessHandlerRemote> frame_access_handler_remote_;
  std::map<int32_t /*client_id*/, ClientContext> clients_;
  std::vector<BufferContext> buffer_contexts_;
  Status status_;
  base::OnceClosure on_stopped_handler_;

  // Keeps track of the last VideoCaptureError that arrived via OnError().
  // This is used for relaying the error event to clients that connect after the
  // OnError() call has been received.
  media::VideoCaptureError error_;

  std::map<
      int32_t,
      std::unique_ptr<
          media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>>
      scoped_access_permissions_by_buffer_context_id_;

  // Keeps track of the id value for the next client to be added. This member is
  // incremented each time a client is added and represents a unique identifier
  // for each client.
  int32_t next_client_id_;

  base::WeakPtrFactory<BroadcastingReceiver> weak_factory_{this};
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_BROADCASTING_RECEIVER_H_
