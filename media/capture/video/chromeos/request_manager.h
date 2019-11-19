// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_REQUEST_MANAGER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_REQUEST_MANAGER_H_

#include <cstring>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "media/capture/mojom/image_capture.mojom.h"
#include "media/capture/video/chromeos/camera_app_device_impl.h"
#include "media/capture/video/chromeos/camera_device_delegate.h"
#include "media/capture/video/chromeos/mojom/camera3.mojom.h"
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#include "media/capture/video/chromeos/request_builder.h"
#include "media/capture/video/chromeos/stream_buffer_manager.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace media {

class CameraBufferFactory;
class CameraDeviceContext;

// The JPEG transport header as defined by Android camera HAL v3 API.  The JPEG
// transport header is at the end of the blob buffer filled by the HAL.
constexpr uint16_t kCamera3JpegBlobId = 0x00FF;
struct Camera3JpegBlob {
  uint16_t jpeg_blob_id;
  uint32_t jpeg_size;
};

// Minimum configured streams should at least contain kPreviewOutput.
constexpr int32_t kMinConfiguredStreams = 1;

// Maximum configured streams could contain two optional YUV streams.
constexpr int32_t kMaxConfiguredStreams = 4;

// Interface that provides API to let Camera3AController to update the metadata
// that will be sent with capture request.
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

// RequestManager is responsible for managing the flow for sending capture
// requests and receiving capture results. Having RequestBuilder to build
// request and StreamBufferManager to handles stream buffers, it focuses on
// controlling the capture flow between Chrome and camera HAL process.
class CAPTURE_EXPORT RequestManager final
    : public cros::mojom::Camera3CallbackOps,
      public CaptureMetadataDispatcher {
 public:
  using BlobifyCallback = base::RepeatingCallback<mojom::BlobPtr(
      const uint8_t* buffer,
      const uint32_t bytesused,
      const VideoCaptureFormat& capture_format,
      int screen_rotation)>;
  using TakePhotoCallback =
      base::OnceCallback<void(int status, media::mojom::BlobPtr blob_result)>;

  // CaptureResult is used to hold the pending capture results for each frame.
  struct CaptureResult {
    CaptureResult();
    ~CaptureResult();
    // The shutter timestamp in nanoseconds.
    uint64_t shutter_timestamp;
    // |reference_time| and |timestamp| are derived from the shutter time of
    // this frame.  They are be passed to |client_->OnIncomingCapturedData|
    // along with the |buffers| when the captured frame is submitted.
    base::TimeTicks reference_time;
    base::TimeDelta timestamp;
    // The result metadata.  Contains various information about the captured
    // frame.
    cros::mojom::CameraMetadataPtr metadata;
    // The buffer handles that hold the captured data of this frame.
    std::map<StreamType, cros::mojom::Camera3StreamBufferPtr> buffers;
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
    TakePhotoCallback still_capture_callback;
    // The reprocess effect that this capture request is used. Will be set to
    // NO_EFFECT if it is not a reprocess request.
    cros::mojom::Effect reprocess_effect;
    // The input buffer id for this capture request.
    base::Optional<uint64_t> input_buffer_id;
  };

  RequestManager(mojo::PendingReceiver<cros::mojom::Camera3CallbackOps>
                     callback_ops_receiver,
                 std::unique_ptr<StreamCaptureInterface> capture_interface,
                 CameraDeviceContext* device_context,
                 VideoCaptureBufferType buffer_type,
                 std::unique_ptr<CameraBufferFactory> camera_buffer_factory,
                 BlobifyCallback blobify_callback,
                 scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner,
                 CameraAppDeviceImpl* camera_app_device);
  ~RequestManager() override;

  // Sets up the stream context and allocate buffers according to the
  // configuration specified in |streams|.
  void SetUpStreamsAndBuffers(
      VideoCaptureFormat capture_format,
      const cros::mojom::CameraMetadataPtr& static_metadata,
      std::vector<cros::mojom::Camera3StreamPtr> streams);

  cros::mojom::Camera3StreamPtr GetStreamConfiguration(StreamType stream_type);

  bool HasStreamsConfiguredForTakePhoto();

  // StartPreview is the entry point to starting the video capture.  The way
  // the video capture loop works is:
  //
  //  (1) Preparing capture request by mixing preview request, one-shot request
  //      and reprocess request if they exists. And build the capture request by
  //      RequestBuilder.
  //  (2) Once the capture request is built, it sends the capture request and
  //      it will go back to (1) to generate next capture request.
  //  (3) The camera HAL returns the shutter time of a capture request through
  //      Notify(), and the filled buffer through ProcessCaptureResult().
  //  (4) Once all the result metadata are collected, it would pass
  //      TrySubmitPendingBuffers() and SubmitCaptureResult() will be triggered
  //      to deliver the filled buffer to Chrome. After the buffer is consumed
  //      by Chrome it is enqueued back to the free buffer queue. Goto (1) to
  //      start another capture loop.
  //
  // When TakePhoto() is called, an additional YUV buffer is queued in step (2)
  // to let the HAL fill the photo result in YUV format. If it is a regular
  // capture, only one reprocess task will be added into the queue which asks
  // HAL to convert YUV photo to JPEG format. If it is a request with
  // special effect (e.g. Portrait mode shot), there will be more than one
  // reprocess task added in the queue and it will be processed sequentially.
  //
  // For every reprocess task, there is a corresponding callback which will
  // return the photo result in JPEG format.
  void StartPreview(cros::mojom::CameraMetadataPtr preview_settings);

  // Stops the capture loop.  After StopPreview is called |callback_ops_| is
  // unbound, so no new capture request or result will be processed. It will
  // also try to trigger Flush() and pass the |callback| to it.
  void StopPreview(base::OnceCallback<void(int32_t)> callback);

  void TakePhoto(cros::mojom::CameraMetadataPtr settings,
                 ReprocessTaskQueue reprocess_tasks);

  base::WeakPtr<RequestManager> GetWeakPtr();

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

 private:
  friend class RequestManagerTest;

  // ReprocessJobInfo holds the queued reprocess tasks and associated metadata
  // for a given YUVInput buffer.
  struct ReprocessJobInfo {
    ReprocessJobInfo(ReprocessTaskQueue queue, uint64_t timestamp);
    ReprocessJobInfo(ReprocessJobInfo&& info);
    ~ReprocessJobInfo();

    ReprocessTaskQueue task_queue;
    uint64_t shutter_timestamp;
  };

  // Puts Jpeg orientation information into the metadata.
  void SetJpegOrientation(cros::mojom::CameraMetadataPtr* settings);

  // Puts sensor timestamp into the metadata for reprocess request.
  void SetSensorTimestamp(cros::mojom::CameraMetadataPtr* settings,
                          uint64_t shutter_timestamp);

  // Puts availability of Zero Shutter Lag into the metadata.
  void SetZeroShutterLag(cros::mojom::CameraMetadataPtr* settings,
                         bool enabled);

  // Prepares a capture request by mixing repeating request with one-shot
  // request if it exists. If there are reprocess requests in the queue, just
  // build the reprocess capture request without mixing the repeating request.
  void PrepareCaptureRequest();

  bool TryPrepareReprocessRequest(std::set<StreamType>* stream_types,
                                  cros::mojom::CameraMetadataPtr* settings,
                                  TakePhotoCallback* callback,
                                  base::Optional<uint64_t>* input_buffer_id,
                                  cros::mojom::Effect* reprocess_effect);

  bool TryPreparePreviewRequest(std::set<StreamType>* stream_types,
                                cros::mojom::CameraMetadataPtr* settings);

  bool TryPrepareOneShotRequest(std::set<StreamType>* stream_types,
                                cros::mojom::CameraMetadataPtr* settings,
                                TakePhotoCallback* callback);

  // Callback for ProcessCaptureRequest().
  void OnProcessedCaptureRequest(int32_t result);

  // ProcessCaptureResult receives the result metadata as well as the filled
  // buffer from camera HAL.  The result metadata may be divided and delivered
  // in several stages.  Before all the result metadata is received the
  // partial results are kept in |pending_results_|.
  void ProcessCaptureResult(
      cros::mojom::Camera3CaptureResultPtr result) override;

  // Checks if the pending buffers are ready to submit. Trigger
  // SubmitCaptureResult() if the buffers are ready to submit.
  void TrySubmitPendingBuffers(uint32_t frame_number);

  // Notify receives the shutter time of capture requests and various errors
  // from camera HAL.  The shutter time is used as the timestamp in the video
  // frame delivered to Chrome.
  void Notify(cros::mojom::Camera3NotifyMsgPtr message) override;

  void HandleNotifyError(uint32_t frame_number,
                         StreamType stream_type,
                         cros::mojom::Camera3ErrorMsgCode error_code);

  // Submits the captured buffer of frame |frame_number_| for the given
  // |stream_type| to Chrome if all the required metadata and the captured
  // buffer are received.  After the buffer is submitted the function then
  // enqueues the buffer to free buffer queue for the next capture request.
  void SubmitCaptureResult(uint32_t frame_number,
                           StreamType stream_type,
                           cros::mojom::Camera3StreamBufferPtr stream_buffer);
  void SubmitCapturedPreviewBuffer(uint32_t frame_number,
                                   uint64_t buffer_ipc_id);
  void SubmitCapturedJpegBuffer(uint32_t frame_number, uint64_t buffer_ipc_id);

  // If there are some metadata set by SetCaptureMetadata() or
  // SetRepeatingCaptureMetadata(), update them onto |capture_settings|.
  void UpdateCaptureSettings(cros::mojom::CameraMetadataPtr* capture_settings);

  mojo::Receiver<cros::mojom::Camera3CallbackOps> callback_ops_;

  std::unique_ptr<StreamCaptureInterface> capture_interface_;

  CameraDeviceContext* device_context_;

  bool video_capture_use_gmb_;

  // StreamBufferManager should be declared before RequestBuilder since
  // RequestBuilder holds an instance of StreamBufferManager and should be
  // destroyed first.
  std::unique_ptr<StreamBufferManager> stream_buffer_manager_;

  std::unique_ptr<RequestBuilder> request_builder_;

  BlobifyCallback blobify_callback_;

  // Where all the Mojo IPC calls takes place.
  const scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;

  // A flag indicating whether the capture loops is running.
  bool capturing_;

  // The number of partial stages.  |partial_result_count_| is learned by
  // querying |static_metadata_|.  In case the result count is absent in
  // |static_metadata_|, it defaults to one which means all the result
  // metadata and captured buffer of a frame are returned together in one
  // shot.
  uint32_t partial_result_count_;

  // The pipeline depth reported in the ANDROID_REQUEST_PIPELINE_MAX_DEPTH
  // metadata.
  size_t pipeline_depth_;

  // The number of preview buffers queued to the camera service.  The request
  // manager needs to try its best to queue |pipeline_depth_| preview buffers to
  // avoid camera frame drops.
  size_t preview_buffers_queued_;

  // The shutter time of the first frame.  We derive the |timestamp| of a
  // frame using the difference between the frame's shutter time and
  // |first_frame_shutter_time_|.
  base::TimeTicks first_frame_shutter_time_;

  // The repeating request settings.  The settings come from the default preview
  // request settings reported by the HAL.  |repeating_request_settings_| is the
  // default settings for each capture request.
  cros::mojom::CameraMetadataPtr repeating_request_settings_;

  // A queue of oneshot request settings.  These are the request settings for
  // each still capture requests.  |oneshot_request_settings_| overrides
  // |repeating_request_settings_| if present.
  std::queue<cros::mojom::CameraMetadataPtr> oneshot_request_settings_;

  // StreamBufferManager does not own the ResultMetadataObservers.  The
  // observers are responsible for removing itself before self-destruction.
  std::unordered_set<ResultMetadataObserver*> result_metadata_observers_;

  // The list of settings to set/override once in the capture request.
  std::vector<cros::mojom::CameraMetadataEntryPtr> capture_settings_override_;

  // The settings to set/override repeatedly in the capture request.  In
  // conflict with |capture_settings_override_|, this one has lower priority.
  std::map<cros::mojom::CameraMetadataTag, cros::mojom::CameraMetadataEntryPtr>
      capture_settings_repeating_override_;

  // Stores the pending capture results of the current in-flight frames.
  std::map<uint32_t, CaptureResult> pending_results_;

  std::queue<cros::mojom::CameraMetadataPtr> take_photo_settings_queue_;

  // Queue that contains ReprocessTaskQueue that will be consumed by
  // reprocess-supported devices.
  std::queue<ReprocessTaskQueue> pending_reprocess_tasks_queue_;

  // Callback for TakePhoto(). When preparing capture request, the callback will
  // be popped and moved to CaptureResult.
  std::queue<base::OnceCallback<void(int, mojom::BlobPtr)>>
      take_photo_callback_queue_;

  // Map that maps buffer id to reprocess task info. If all reprocess tasks for
  // specific buffer id are all consumed, release that buffer.
  std::map<uint64_t, ReprocessJobInfo> buffer_id_reprocess_job_info_map_;

  // Map that maps frame number to reprocess task queue. We should consume the
  // content inside this map when preparing capture request.
  std::map<uint32_t, ReprocessTaskQueue> frame_number_reprocess_tasks_map_;

  // Buffer ids that are currently processing. When preparing capture request,
  // we will ignore the reprocess task if its corresponding buffer id is in
  // the set.
  std::set<uint64_t> processing_buffer_ids_;

  // Map for retrieving the last received frame number. It is used to check for
  // duplicate or out of order of frames.
  std::map<StreamType, uint32_t> last_received_frame_number_map_;

  CameraAppDeviceImpl* camera_app_device_;  // Weak.

  base::WeakPtrFactory<RequestManager> weak_ptr_factory_{this};

  DISALLOW_IMPLICIT_CONSTRUCTORS(RequestManager);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_REQUEST_MANAGER_H_
