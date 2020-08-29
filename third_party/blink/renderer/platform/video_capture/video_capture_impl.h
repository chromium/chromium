// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_VIDEO_CAPTURE_VIDEO_CAPTURE_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_VIDEO_CAPTURE_VIDEO_CAPTURE_IMPL_H_

#include <stdint.h>
#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/video_frame.h"
#include "media/capture/mojom/video_capture.mojom-blink.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/media/video_capture.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace gpu {
class GpuMemoryBufferSupport;
}  // namespace gpu

namespace media {
class GpuVideoAcceleratorFactories;
}  // namespace media

namespace blink {

// VideoCaptureImpl represents a capture device in renderer process. It provides
// an interface for clients to command the capture (Start, Stop, etc), and
// communicates back to these clients e.g. the capture state or incoming
// captured VideoFrames. VideoCaptureImpl is created in the main Renderer thread
// but otherwise operates on |io_task_runner_|, which is usually the IO thread.
class PLATFORM_EXPORT VideoCaptureImpl
    : public media::mojom::blink::VideoCaptureObserver {
 public:
  explicit VideoCaptureImpl(media::VideoCaptureSessionId session_id);
  ~VideoCaptureImpl() override;

  // Stop/resume delivering video frames to clients, based on flag |suspend|.
  void SuspendCapture(bool suspend);

  // Start capturing using the provided parameters.
  // |client_id| must be unique to this object in the render process. It is
  // used later to stop receiving video frames.
  // |state_update_cb| will be called when state changes.
  // |deliver_frame_cb| will be called when a frame is ready.
  void StartCapture(int client_id,
                    const media::VideoCaptureParams& params,
                    const VideoCaptureStateUpdateCB& state_update_cb,
                    const VideoCaptureDeliverFrameCB& deliver_frame_cb);

  // Stop capturing. |client_id| is the identifier used to call StartCapture.
  void StopCapture(int client_id);

  // Requests that the video capturer send a frame "soon" (e.g., to resolve
  // picture loss or quality issues).
  void RequestRefreshFrame();

  // Get capturing formats supported by this device.
  // |callback| will be invoked with the results.
  //
  using VideoCaptureDeviceFormatsCallback =
      base::OnceCallback<void(const Vector<media::VideoCaptureFormat>&)>;
  void GetDeviceSupportedFormats(VideoCaptureDeviceFormatsCallback callback);

  // Get capturing formats currently in use by this device.
  // |callback| will be invoked with the results.
  void GetDeviceFormatsInUse(VideoCaptureDeviceFormatsCallback callback);

  void OnFrameDropped(media::VideoCaptureFrameDropReason reason);
  void OnLog(const String& message);

  const media::VideoCaptureSessionId& session_id() const { return session_id_; }

  void SetVideoCaptureHostForTesting(
      media::mojom::blink::VideoCaptureHost* service) {
    video_capture_host_for_testing_ = service;
  }
  void SetGpuMemoryBufferSupportForTesting(
      std::unique_ptr<gpu::GpuMemoryBufferSupport> gpu_memory_buffer_support);

  // media::mojom::VideoCaptureObserver implementation.
  void OnStateChanged(media::mojom::VideoCaptureState state) override;
  void OnNewBuffer(
      int32_t buffer_id,
      media::mojom::blink::VideoBufferHandlePtr buffer_handle) override;
  void OnBufferReady(int32_t buffer_id,
                     media::mojom::blink::VideoFrameInfoPtr info) override;
  void OnBufferDestroyed(int32_t buffer_id) override;

 private:
  friend class VideoCaptureImplTest;
  friend class MockVideoCaptureImpl;

  struct BufferContext;

  // Contains information about a video capture client, including capture
  // parameters callbacks to the client.
  struct ClientInfo;
  using ClientInfoMap = std::map<int, ClientInfo>;

  using BufferFinishedCallback =
      base::OnceCallback<void(media::VideoFrameFeedback feedback)>;

  void OnVideoFrameReady(int32_t buffer_id,
                         base::TimeTicks reference_time,
                         media::mojom::blink::VideoFrameInfoPtr info,
                         scoped_refptr<media::VideoFrame> frame,
                         scoped_refptr<BufferContext> buffer_context);

  void OnAllClientsFinishedConsumingFrame(
      int buffer_id,
      scoped_refptr<BufferContext> buffer_context,
      media::VideoFrameFeedback feedback);

  void StopDevice();
  void RestartCapture();
  void StartCaptureInternal();

  void OnDeviceSupportedFormats(
      VideoCaptureDeviceFormatsCallback callback,
      const Vector<media::VideoCaptureFormat>& supported_formats);

  void OnDeviceFormatsInUse(
      VideoCaptureDeviceFormatsCallback callback,
      const Vector<media::VideoCaptureFormat>& formats_in_use);

  // Tries to remove |client_id| from |clients|, returning false if not found.
  bool RemoveClient(int client_id, ClientInfoMap* clients);

  media::mojom::blink::VideoCaptureHost* GetVideoCaptureHost();

  // Called (by an unknown thread) when all consumers are done with a VideoFrame
  // and its ref-count has gone to zero.  This helper function grabs the
  // RESOURCE_UTILIZATION value from the |metadata| and then runs the given
  // callback, to trampoline back to the IO thread with the values.
  static void DidFinishConsumingFrame(
      const media::VideoFrameFeedback* feedback,
      BufferFinishedCallback callback_to_io_thread);

  // |device_id_| and |session_id_| are different concepts, but we reuse the
  // same numerical value, passed on construction.
  const base::UnguessableToken device_id_;
  const base::UnguessableToken session_id_;

  // |video_capture_host_| is an IO-thread mojo::Remote to a remote service
  // implementation and is created by binding |pending_video_capture_host_|,
  // unless a |video_capture_host_for_testing_| has been injected.
  mojo::PendingRemote<media::mojom::blink::VideoCaptureHost>
      pending_video_capture_host_;
  mojo::Remote<media::mojom::blink::VideoCaptureHost> video_capture_host_;
  media::mojom::blink::VideoCaptureHost* video_capture_host_for_testing_;

  mojo::Receiver<media::mojom::blink::VideoCaptureObserver> observer_receiver_{
      this};

  // Buffers available for sending to the client.
  using ClientBufferMap = std::map<int32_t, scoped_refptr<BufferContext>>;
  ClientBufferMap client_buffers_;

  ClientInfoMap clients_;
  ClientInfoMap clients_pending_on_restart_;

  // Video format requested by the client to this class via StartCapture().
  media::VideoCaptureParams params_;

  // First captured frame reference time sent from browser process side.
  base::TimeTicks first_frame_ref_time_;

  VideoCaptureState state_;

  int num_first_frame_logs_ = 0;

  // Methods of |gpu_factories_| need to run on |media_task_runner_|.
  media::GpuVideoAcceleratorFactories* gpu_factories_;
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  std::unique_ptr<gpu::GpuMemoryBufferSupport> gpu_memory_buffer_support_;

  THREAD_CHECKER(io_thread_checker_);

  // WeakPtrFactory pointing back to |this| object, for use with
  // media::VideoFrames constructed in OnBufferReceived() from buffers cached
  // in |client_buffers_|.
  base::WeakPtrFactory<VideoCaptureImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_VIDEO_CAPTURE_VIDEO_CAPTURE_IMPL_H_
