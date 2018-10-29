// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_STREAM_BUFFER_MANAGER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_STREAM_BUFFER_MANAGER_H_

#include <memory>
#include <queue>
#include <unordered_map>
#include <vector>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "media/capture/video/chromeos/camera_device_delegate.h"
#include "media/capture/video/chromeos/mojo/camera3.mojom.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace gfx {

class GpuMemoryBuffer;

}  // namespace base

namespace media {

class CameraBufferFactory;
class CameraDeviceContext;

// One stream for preview, one stream for still capture.
constexpr size_t kMaxConfiguredStreams = 2;

// The JPEG transport header as defined by Android camera HAL v3 API.  The JPEG
// transport header is at the end of the blob buffer filled by the HAL.
constexpr uint16_t kCamera3JpegBlobId = 0x00FF;
struct Camera3JpegBlob {
  uint16_t jpeg_blob_id;
  uint32_t jpeg_size;
};

class CAPTURE_EXPORT CaptureMetadataDispatcher {
 public:
  class ResultMetadataObserver {
   public:
    virtual ~ResultMetadataObserver() {}
    virtual void OnResultMetadataAvailable(
        const cros::mojom::CameraMetadataPtr&) = 0;
  };

  virtual ~CaptureMetadataDispatcher() {}
  virtual void AddResultMetadataObserver(ResultMetadataObserver* observer) = 0;
  virtual void RemoveResultMetadataObserver(
      ResultMetadataObserver* observer) = 0;
  virtual void SetCaptureMetadata(cros::mojom::CameraMetadataTag tag,
                                  cros::mojom::EntryType type,
                                  size_t count,
                                  std::vector<uint8_t> value) = 0;
  virtual void SetRepeatingCaptureMetadata(cros::mojom::CameraMetadataTag tag,
                                           cros::mojom::EntryType type,
                                           size_t count,
                                           std::vector<uint8_t> value) = 0;
  virtual void UnsetRepeatingCaptureMetadata(
      cros::mojom::CameraMetadataTag tag) = 0;
};

// StreamBufferManager is responsible for managing the buffers of the
// stream.  StreamBufferManager allocates buffers according to the given
// stream configuration, and circulates the buffers along with capture
// requests and results between Chrome and the camera HAL process.
class CAPTURE_EXPORT StreamBufferManager final
    : public cros::mojom::Camera3CallbackOps,
      public CaptureMetadataDispatcher {
 public:
  StreamBufferManager(
      cros::mojom::Camera3CallbackOpsRequest callback_ops_request,
      std::unique_ptr<StreamCaptureInterface> capture_interface,
      CameraDeviceContext* device_context,
      std::unique_ptr<CameraBufferFactory> camera_buffer_factory,
      base::RepeatingCallback<
          mojom::BlobPtr(const uint8_t* buffer,
                         const uint32_t bytesused,
                         const VideoCaptureFormat& capture_format,
                         int screen_rotation)> blobify_callback,
      scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner);

  ~StreamBufferManager() override;

  // Sets up the stream context and allocate buffers according to the
  // configuration specified in |stream|.
  void SetUpStreamsAndBuffers(
      VideoCaptureFormat capture_format,
      const cros::mojom::CameraMetadataPtr& static_metadata,
      std::vector<cros::mojom::Camera3StreamPtr> streams);

  // StartPreview is the entry point to starting the video capture.  The way
  // the video capture loop works is:
  //
  //  (1) If there is a free buffer, RegisterBuffer registers the buffer with
  //      the camera HAL.
  //  (2) Once the free buffer is registered, ProcessCaptureRequest is called
  //      to issue a capture request which will eventually fill the registered
  //      buffer.  Goto (1) to register the remaining free buffers.
  //  (3) The camera HAL returns the shutter time of a capture request through
  //      Notify, and the filled buffer through ProcessCaptureResult.
  //  (4) Once all the result metadata are collected,
  //      SubmitCaptureResultIfComplete is called to deliver the filled buffer
  //      to Chrome.  After the buffer is consumed by Chrome it is enqueued back
  //      to the free buffer queue.  Goto (1) to start another capture loop.
  //
  // When TakePhoto() is called, an additional BLOB buffer is queued in step (2)
  // to let the HAL fill the still capture JPEG image.  When the JPEG image is
  // returned in (4), it's passed to upper layer through the TakePhotoCallback.
  void StartPreview(cros::mojom::CameraMetadataPtr preview_settings);

  // Stops the capture loop.  After StopPreview is called |callback_ops_| is
  // unbound, so no new capture request or result will be processed.
  void StopPreview(base::OnceCallback<void(int32_t)> callback);

  cros::mojom::Camera3StreamPtr GetStreamConfiguration(StreamType stream_type);

  void TakePhoto(cros::mojom::CameraMetadataPtr settings,
                 VideoCaptureDevice::TakePhotoCallback callback);

  size_t GetStreamNumber();

  // CaptureMetadataDispatcher implementations.
  void AddResultMetadataObserver(ResultMetadataObserver* observer) override;
  void RemoveResultMetadataObserver(ResultMetadataObserver* observer) override;

  // Queues a capture setting that will be send along with the earliest next
  // capture request.
  void SetCaptureMetadata(cros::mojom::CameraMetadataTag tag,
                          cros::mojom::EntryType type,
                          size_t count,
                          std::vector<uint8_t> value) override;

  void SetRepeatingCaptureMetadata(cros::mojom::CameraMetadataTag tag,
                                   cros::mojom::EntryType type,
                                   size_t count,
                                   std::vector<uint8_t> value) override;

  void UnsetRepeatingCaptureMetadata(
      cros::mojom::CameraMetadataTag tag) override;

  static uint64_t GetBufferIpcId(StreamType stream_type, size_t index);

 private:
  friend class StreamBufferManagerTest;

  // Registers a free buffer, if any, for the give |stream_type| to the camera
  // HAL.
  void RegisterBuffer(StreamType stream_type);

  // Calls ProcessCaptureRequest if the buffer specified by |buffer_id| is
  // successfully registered.
  void OnRegisteredBuffer(StreamType stream_type,
                          uint64_t buffer_id,
                          int32_t result);

  // The capture request contains the buffer handles waiting to be filled.
  void ProcessCaptureRequest();
  // Calls RegisterBuffer to attempt to register any remaining free buffers.
  void OnProcessedCaptureRequest(int32_t result);

  // Camera3CallbackOps implementations.

  // ProcessCaptureResult receives the result metadata as well as the filled
  // buffer from camera HAL.  The result metadata may be divided and delivered
  // in several stages.  Before all the result metadata is received the
  // partial results are kept in |pending_results_|.
  void ProcessCaptureResult(
      cros::mojom::Camera3CaptureResultPtr result) override;

  // Notify receives the shutter time of capture requests and various errors
  // from camera HAL.  The shutter time is used as the timestamp in the video
  // frame delivered to Chrome.
  void Notify(cros::mojom::Camera3NotifyMsgPtr message) override;
  void HandleNotifyError(uint32_t frame_number,
                         StreamType stream_type,
                         cros::mojom::Camera3ErrorMsgCode error_code);

  // Submits the captured buffer of frame |frame_number_| for the give
  // |stream_type| to Chrome if all the required metadata and the captured
  // buffer are received.  After the buffer is submitted the function then
  // enqueues the buffer to free buffer queue for the next capture request.
  void SubmitCaptureResultIfComplete(uint32_t frame_number,
                                     StreamType stream_type);
  void SubmitCaptureResult(uint32_t frame_number, StreamType stream_type);

  void ApplyCaptureSettings(cros::mojom::CameraMetadataPtr* capture_settings);

  mojo::Binding<cros::mojom::Camera3CallbackOps> callback_ops_;

  std::unique_ptr<StreamCaptureInterface> capture_interface_;

  CameraDeviceContext* device_context_;

  std::unique_ptr<CameraBufferFactory> camera_buffer_factory_;

  base::RepeatingCallback<mojom::BlobPtr(
      const uint8_t* buffer,
      const uint32_t bytesused,
      const VideoCaptureFormat& capture_format,
      int screen_rotation)>
      blobify_callback_;

  // Where all the Mojo IPC calls takes place.
  const scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;

  // A flag indicating whether the capture loops is running.
  bool capturing_;

  // The frame number.  Increased by one for each capture request sent; reset
  // to zero in AllocateAndStart.
  uint32_t frame_number_;

  // CaptureResult is used to hold the pending capture results for each frame.
  struct CaptureResult {
    CaptureResult();
    ~CaptureResult();
    // |reference_time| and |timestamp| are derived from the shutter time of
    // this frame.  They are be passed to |client_->OnIncomingCapturedData|
    // along with the |buffers| when the captured frame is submitted.
    base::TimeTicks reference_time;
    base::TimeDelta timestamp;
    // The result metadata.  Contains various information about the captured
    // frame.
    cros::mojom::CameraMetadataPtr metadata;
    // The buffer handles that hold the captured data of this frame.
    std::unordered_map<StreamType, cros::mojom::Camera3StreamBufferPtr> buffers;
    // The set of the partial metadata received.  For each capture result, the
    // total number of partial metadata should equal to
    // |partial_result_count_|.
    std::set<uint32_t> partial_metadata_received;
    // Incremented for every stream buffer requested for the given frame.
    // StreamBufferManager destructs the CaptureResult when
    // |unsubmitted_buffer_count| drops to zero.
    size_t unsubmitted_buffer_count;
    // The callback used to return the captured still capture JPEG buffer.  Set
    // if and only if the capture request was sent with a still capture buffer.
    VideoCaptureDevice::TakePhotoCallback still_capture_callback;
  };

  struct StreamContext {
    StreamContext();
    ~StreamContext();
    // The actual pixel format used in the capture request.
    VideoCaptureFormat capture_format;
    // The camera HAL stream.
    cros::mojom::Camera3StreamPtr stream;
    // The allocated buffers of this stream.
    std::vector<std::unique_ptr<gfx::GpuMemoryBuffer>> buffers;
    // The free buffers of this stream.  The queue stores indices into the
    // |buffers| vector.
    std::queue<uint64_t> free_buffers;
    // The buffers that are registered to the HAL, which can be used as the
    // output buffers for capture requests.
    std::queue<uint64_t> registered_buffers;
    // The pointers to the pending capture results that have unsubmitted result
    // buffers.
    std::map<uint32_t, CaptureResult*> capture_results_with_buffer;
  };

  // The context for the set of active streams.
  std::unordered_map<StreamType, std::unique_ptr<StreamContext>>
      stream_context_;

  // The repeating request settings.  The settings come from the default preview
  // request settings reported by the HAL.  |repeating_request_settings_| is the
  // default settings for each capture request.
  cros::mojom::CameraMetadataPtr repeating_request_settings_;

  // A queue of oneshot request settings.  These are the request settings for
  // each still capture requests.  |oneshot_request_settings_| overrides
  // |repeating_request_settings_| if present.
  std::queue<cros::mojom::CameraMetadataPtr> oneshot_request_settings_;

  // The pending callbacks for the TakePhoto requests.
  std::queue<VideoCaptureDevice::TakePhotoCallback>
      still_capture_callbacks_yet_to_be_processed_;
  std::queue<VideoCaptureDevice::TakePhotoCallback>
      still_capture_callbacks_currently_processing_;

  // The number of partial stages.  |partial_result_count_| is learned by
  // querying |static_metadata_|.  In case the result count is absent in
  // |static_metadata_|, it defaults to one which means all the result
  // metadata and captured buffer of a frame are returned together in one
  // shot.
  uint32_t partial_result_count_;

  // The shutter time of the first frame.  We derive the |timestamp| of a
  // frame using the difference between the frame's shutter time and
  // |first_frame_shutter_time_|.
  base::TimeTicks first_frame_shutter_time_;

  // Stores the pending capture results of the current in-flight frames.
  std::map<uint32_t, CaptureResult> pending_results_;

  // StreamBufferManager does not own the ResultMetadataObservers.  The
  // observers are responsible for removing itself before self-destruction.
  std::unordered_set<ResultMetadataObserver*> result_metadata_observers_;

  // The list of settings to set/override once in the capture request.
  std::vector<cros::mojom::CameraMetadataEntryPtr> capture_settings_override_;

  // The settings to set/override repeatedly in the capture request.  In
  // conflict with |capture_settings_override_|, this one has lower priority.
  std::map<cros::mojom::CameraMetadataTag, cros::mojom::CameraMetadataEntryPtr>
      capture_settings_repeating_override_;

  base::WeakPtrFactory<StreamBufferManager> weak_ptr_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StreamBufferManager);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_STREAM_BUFFER_MANAGER_H_
