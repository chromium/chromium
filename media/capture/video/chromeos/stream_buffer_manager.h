// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_STREAM_BUFFER_MANAGER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_STREAM_BUFFER_MANAGER_H_

#include <cstring>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <unordered_map>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "media/capture/video/chromeos/camera_device_context.h"
#include "media/capture/video/chromeos/camera_device_delegate.h"
#include "media/capture/video/chromeos/mojom/camera3.mojom.h"
#include "media/capture/video_capture_types.h"

namespace gfx {

class GpuMemoryBuffer;

}  // namespace gfx

namespace gpu {

class GpuMemoryBufferImpl;
class GpuMemoryBufferSupport;

}  // namespace gpu

namespace media {

class CameraBufferFactory;
class VideoCaptureBufferObserver;

struct BufferInfo;

// StreamBufferManager is responsible for managing the buffers of the
// stream.  StreamBufferManager allocates buffers according to the given
// stream configuration.
class CAPTURE_EXPORT StreamBufferManager final {
 public:
  using Buffer = VideoCaptureDevice::Client::Buffer;

  StreamBufferManager() = delete;

  StreamBufferManager(
      CameraDeviceContext* device_context,
      bool video_capture_use_gmb,
      std::unique_ptr<CameraBufferFactory> camera_buffer_factory,
      std::unique_ptr<VideoCaptureBufferObserver> buffer_observer);

  StreamBufferManager(const StreamBufferManager&) = delete;
  StreamBufferManager& operator=(const StreamBufferManager&) = delete;

  ~StreamBufferManager();

  void ReserveBuffer(StreamType stream_type);

  gfx::GpuMemoryBuffer* GetGpuMemoryBufferById(StreamType stream_type,
                                               uint64_t buffer_ipc_id);

  // Acquires the VCD client buffer specified by |stream_type| and
  // |buffer_ipc_id|, with optional rotation applied.  |rotation| is the
  // clockwise degrees that the source frame would be rotated to, and the valid
  // values are 0, 90, 180, and 270.  Returns the VideoCaptureFormat of the
  // returned buffer in |format|.
  //
  // TODO(crbug.com/990682): Remove the |rotation| arg when we disable the
  // camera frame rotation for good.
  std::optional<Buffer> AcquireBufferForClientById(StreamType stream_type,
                                                   uint64_t buffer_ipc_id,
                                                   VideoCaptureFormat* format);

  VideoCaptureFormat GetStreamCaptureFormat(StreamType stream_type);

  // Checks if all streams are available. For output stream, it is available if
  // it has free buffers. For input stream, it is always available.
  bool HasFreeBuffers(const std::set<StreamType>& stream_types);

  // Gets the number of free buffers for the stream specified by |stream_type|.
  size_t GetFreeBufferCount(StreamType stream_type);

  // Checks if the target stream types have been configured or not.
  bool HasStreamsConfigured(std::initializer_list<StreamType> stream_types);

  // Sets up the stream context and allocate buffers according to the
  // configuration specified in |stream|.
  void SetUpStreamsAndBuffers(
      base::flat_map<ClientType, VideoCaptureParams> capture_params,
      const cros::mojom::CameraMetadataPtr& static_metadata,
      std::vector<cros::mojom::Camera3StreamPtr> streams);

  cros::mojom::Camera3StreamPtr GetStreamConfiguration(StreamType stream_type);

  // Requests buffer for specific stream type.
  std::optional<BufferInfo> RequestBufferForCaptureRequest(
      StreamType stream_type);

  // Releases buffer by marking it as free buffer.
  void ReleaseBufferFromCaptureResult(StreamType stream_type,
                                      uint64_t buffer_ipc_id);

  gfx::Size GetBufferDimension(StreamType stream_type);

  bool IsPortraitModeSupported();

  bool IsRecordingSupported();

  std::unique_ptr<gpu::GpuMemoryBufferImpl> CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferHandle handle,
      const VideoCaptureFormat& format,
      gfx::BufferUsage buffer_usage);

 private:
  friend class RequestManagerTest;

  // BufferPair holding up to two types of handles of a stream buffer.
  struct BufferPair {
    BufferPair(std::unique_ptr<gfx::GpuMemoryBuffer> gmb,
               std::optional<Buffer> vcd_buffer);
    BufferPair(BufferPair&& other);
    ~BufferPair();
    // The GpuMemoryBuffer interface of the stream buffer.
    //   - When the VCD runs SharedMemory-based VideoCapture buffer, |gmb| is
    //     allocated by StreamBufferManager locally.
    //   - When the VCD runs GpuMemoryBuffer-based VideoCapture buffer, |gmb| is
    //     constructed from |vcd_buffer| below.
    std::unique_ptr<gfx::GpuMemoryBuffer> gmb;
    // The VCD buffer reserved from the VCD buffer pool.  This is only set when
    // the VCD runs GpuMemoryBuffer-based VideoCapture buffer.
    std::optional<Buffer> vcd_buffer;
  };

  struct StreamContext {
    StreamContext();
    ~StreamContext();
    // The actual pixel format used in the capture request.
    VideoCaptureFormat capture_format;
    // The camera HAL stream.
    cros::mojom::Camera3StreamPtr stream;
    // The dimension of the buffer layout.
    gfx::Size buffer_dimension;
    // The usage of the buffer.
    gfx::BufferUsage buffer_usage;
    // The allocated buffer pairs.
    std::map<int, BufferPair> buffers;
    // The free buffers of this stream.  The queue stores keys into the
    // |buffers| map.
    std::queue<int> free_buffers;
  };

  static uint64_t GetBufferIpcId(StreamType stream_type, int key);

  static int GetBufferKey(uint64_t buffer_ipc_id);

  bool CanReserveBufferFromPool(StreamType stream_type);
  void ReserveBufferFromFactory(StreamType stream_type);
  void ReserveBufferFromPool(StreamType stream_type);
  // Destroy current streams and unmap mapped buffers.
  void DestroyCurrentStreamsAndBuffers();

  // The context for the set of active streams.
  std::unordered_map<StreamType, std::unique_ptr<StreamContext>>
      stream_context_;

  raw_ptr<CameraDeviceContext> device_context_;

  // The interface to notify camera device a new buffer needs to be registered
  // or a buffer needs to be retired.
  std::unique_ptr<VideoCaptureBufferObserver> buffer_observer_;

  bool video_capture_use_gmb_;

  std::unique_ptr<gpu::GpuMemoryBufferSupport> gmb_support_;

  std::unique_ptr<CameraBufferFactory> camera_buffer_factory_;

  base::WeakPtrFactory<StreamBufferManager> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_STREAM_BUFFER_MANAGER_H_
