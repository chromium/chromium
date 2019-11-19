// Copyright 2018 The Chromium Authors. All rights reserved.
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
#include "services/video_capture/public/mojom/scoped_access_permission.mojom.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"

namespace video_capture {

// Implementation of mojom::VideoFrameHandler that distributes frames to
// potentially multiple clients.
class BroadcastingReceiver : public mojom::VideoFrameHandler {
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
    void set_access_permission(
        mojo::PendingRemote<mojom::ScopedAccessPermission> access_permission) {
      access_permission_.Bind(std::move(access_permission));
    }
    void IncreaseConsumerCount();
    void DecreaseConsumerCount();
    bool IsStillBeingConsumed() const;
    bool is_retired() const { return is_retired_; }
    void set_retired() { is_retired_ = true; }
    media::mojom::VideoBufferHandlePtr CloneBufferHandle(
        media::VideoCaptureBufferType target_buffer_type);

   private:
    // If the source handle is shared_memory_via_raw_file_descriptor, we first
    // have to unwrap it before we can clone it. Instead of unwrapping, cloning,
    // and than wrapping back each time we need to clone it, we convert it to
    // a regular shared memory and keep it in this form.
    void ConvertRawFileDescriptorToSharedBuffer();

    int32_t buffer_context_id_;
    int32_t buffer_id_;
    media::mojom::VideoBufferHandlePtr buffer_handle_;
    // Indicates how many consumers are currently relying on
    // |access_permission_|.
    int32_t consumer_hold_count_;
    bool is_retired_;
    mojo::Remote<mojom::ScopedAccessPermission> access_permission_;
  };

  BroadcastingReceiver();
  ~BroadcastingReceiver() override;

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

  // video_capture::mojom::VideoFrameHandler:
  void OnNewBuffer(int32_t buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override;
  void OnFrameReadyInBuffer(
      int32_t buffer_id,
      int32_t frame_feedback_id,
      mojo::PendingRemote<mojom::ScopedAccessPermission> access_permission,
      media::mojom::VideoFrameInfoPtr frame_info) override;
  void OnBufferRetired(int32_t buffer_id) override;
  void OnError(media::VideoCaptureError error) override;
  void OnFrameDropped(media::VideoCaptureFrameDropReason reason) override;
  void OnLog(const std::string& message) override;
  void OnStarted() override;
  void OnStartedUsingGpuDecode() override;
  void OnStopped() override;

 private:
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

   private:
    mojo::Remote<mojom::VideoFrameHandler> client_;
    media::VideoCaptureBufferType target_buffer_type_;
    bool is_suspended_;
    bool on_started_has_been_called_;
    bool on_started_using_gpu_decode_has_been_called_;
  };

  void OnClientFinishedConsumingFrame(int32_t buffer_context_id);
  void OnClientDisconnected(int32_t client_id);
  std::vector<BufferContext>::iterator FindUnretiredBufferContextFromBufferId(
      int32_t buffer_id);

  SEQUENCE_CHECKER(sequence_checker_);
  std::map<int32_t /*client_id*/, ClientContext> clients_;
  std::vector<BufferContext> buffer_contexts_;
  Status status_;
  base::OnceClosure on_stopped_handler_;

  // Keeps track of the last VideoCaptureError that arrived via OnError().
  // This is used for relaying the error event to clients that connect after the
  // OnError() call has been received.
  media::VideoCaptureError error_;

  // Keeps track of the id value for the next client to be added. This member is
  // incremented each time a client is added and represents a unique identifier
  // for each client.
  int32_t next_client_id_;

  base::WeakPtrFactory<BroadcastingReceiver> weak_factory_{this};
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_BROADCASTING_RECEIVER_H_
