// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_REQUEST_MANAGER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_REQUEST_MANAGER_H_

#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "media/capture/mojom/image_capture.mojom.h"
#include "media/capture/video/chromeos/camera_app_device_impl.h"
#include "media/capture/video/chromeos/camera_device_context.h"
#include "media/capture/video/chromeos/camera_device_delegate.h"
#include "media/capture/video/chromeos/capture_metadata_dispatcher.h"
#include "media/capture/video/chromeos/mojom/camera3.mojom.h"
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#include "media/capture/video/chromeos/request_builder.h"
#include "media/capture/video/chromeos/stream_buffer_manager.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace media {

class CameraBufferFactory;

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

// The interface to register/retire buffer from the buffer pool maintained in
// the camera HAL side.
class CAPTURE_EXPORT VideoCaptureBufferObserver {
 public:
  VideoCaptureBufferObserver(base::WeakPtr<RequestManager> request_manager);

  ~VideoCaptureBufferObserver();

  // Registers buffer to the camera HAL buffer pool.
  void OnNewBuffer(ClientType client_type,
                   cros::mojom::CameraBufferHandlePtr buffer);

  // Retires a buffer from the camera HAL buffer pool.
  void OnBufferRetired(ClientType client_type, uint64_t buffer_id);

 private:
  const base::WeakPtr<RequestManager> request_manager_;
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
    VideoCaptureDevice::TakePhotoCallback still_capture_callback;
    // This map stores callbacks that can be used to return buffers for portrait
    // mode requests.
    TakePhotoCallbackMap portrait_callbacks_map;
  };

  RequestManager() = delete;

  RequestManager(const std::string& device_id,
                 mojo::PendingReceiver<cros::mojom::Camera3CallbackOps>
                     callback_ops_receiver,
                 std::unique_ptr<StreamCaptureInterface> capture_interface,
                 CameraDeviceContext* device_context,
                 VideoCaptureBufferType buffer_type,
                 std::unique_ptr<CameraBufferFactory> camera_buffer_factory,
                 BlobifyCallback blobify_callback,
                 scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner,
                 uint32_t device_api_version,
                 bool use_buffer_management_apis);

  RequestManager(const RequestManager&) = delete;
  RequestManager& operator=(const RequestManager&) = delete;

  ~RequestManager() override;

  // Sets up the stream context and allocate buffers according to the
  // configuration specified in |streams|.
  void SetUpStreamsAndBuffers(
      base::flat_map<ClientType, VideoCaptureParams> capture_params,
      const cros::mojom::CameraMetadataPtr& static_metadata,
      std::vector<cros::mojom::Camera3StreamPtr> streams);

  cros::mojom::Camera3StreamPtr GetStreamConfiguration(StreamType stream_type);

  bool HasStreamsConfiguredForTakePhoto();

  // StartPreview is the entry point to starting the video capture. The way
  // the video capture loop works is:
  //
  //  (1) Preparing capture request by mixing preview request, one-shot request
  //      and effect request if they exists. And build the capture request by
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
  void StartPreview(cros::mojom::CameraMetadataPtr preview_settings);

  // Stops the capture loop.  After StopPreview is called |callback_ops_| is
  // unbound, so no new capture request or result will be processed. It will
  // also try to trigger Flush() and pass the |callback| to it.
  void StopPreview(base::OnceCallback<void(int32_t)> callback);

  void TakePhoto(cros::mojom::CameraMetadataPtr settings,
                 VideoCaptureDevice::TakePhotoCallback callback);

  void TakePortraitPhoto(cros::mojom::CameraMetadataPtr settings,
                         TakePhotoCallbackMap callbacks_map);

  base::WeakPtr<RequestManager> GetWeakPtr();

  // CaptureMetadataDispatcher implementations.
  void AddResultMetadataObserver(ResultMetadataObserver* observer) override;
  void RemoveResultMetadataObserver(ResultMetadataObserver* observer) override;

  // Registers buffer to the camera HAL buffer pool.
  void OnNewBuffer(ClientType client_type,
                   cros::mojom::CameraBufferHandlePtr buffer);

  // Retires a buffer from the camera HAL buffer pool.
  void OnBufferRetired(ClientType client_type, uint64_t buffer_id);

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

  // Puts Portrait Mode vendor tag into the metadata.
  void SetPortraitModeVendorKey(cros::mojom::CameraMetadataPtr* settings);

  // Puts JPEG orientation information into the metadata.
  void SetJpegOrientation(cros::mojom::CameraMetadataPtr* settings,
                          int32_t orientation);

  // Puts JPEG thumbnail size information into the metadata.
  void SetJpegThumbnailSize(cros::mojom::CameraMetadataPtr* settings) const;

  // Puts availability of Zero Shutter Lag into the metadata.
  void SetZeroShutterLag(cros::mojom::CameraMetadataPtr* settings,
                         bool enabled);

  // Prepares a capture request by mixing repeating request with one-shot/effect
  // request if it exists.
  void PrepareCaptureRequest();

  bool TryPreparePortraitModeRequest(std::set<StreamType>* stream_types,
                                     cros::mojom::CameraMetadataPtr* settings,
                                     TakePhotoCallbackMap* callbacks_map);

  bool TryPreparePreviewRequest(std::set<StreamType>* stream_types,
                                cros::mojom::CameraMetadataPtr* settings);

  bool TryPrepareOneShotRequest(
      std::set<StreamType>* stream_types,
      cros::mojom::CameraMetadataPtr* settings,
      VideoCaptureDevice::TakePhotoCallback* callback);

  bool TryPrepareRecordingRequest(std::set<StreamType>* stream_types);

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

  // RequestStreamBuffers receives output buffer requests and a callback to
  // receive results.
  void RequestStreamBuffers(
      std::vector<cros::mojom::Camera3BufferRequestPtr> buffer_reqs,
      RequestStreamBuffersCallback callback) override;

  // ReturnStreamBuffers receives returned output buffers.
  void ReturnStreamBuffers(
      std::vector<cros::mojom::Camera3StreamBufferPtr> buffers) override;

  // Submits the captured buffer of frame |frame_number_| for the given
  // |stream_type| to Chrome if all the required metadata and the captured
  // buffer are received.  After the buffer is submitted the function then
  // enqueues the buffer to free buffer queue for the next capture request.
  void SubmitCaptureResult(uint32_t frame_number,
                           StreamType stream_type,
                           cros::mojom::Camera3StreamBufferPtr stream_buffer);
  void SubmitCapturedPreviewRecordingBuffer(uint32_t frame_number,
                                            uint64_t buffer_ipc_id,
                                            StreamType stream_type);
  void SubmitCapturedJpegBuffer(uint32_t frame_number,
                                uint64_t buffer_ipc_id,
                                StreamType stream_type);

  // If there are some metadata set by SetCaptureMetadata() or
  // SetRepeatingCaptureMetadata(), update them onto |capture_settings|.
  void UpdateCaptureSettings(cros::mojom::CameraMetadataPtr* capture_settings);

  // The unique device id which is retrieved from VideoCaptureDeviceDescriptor.
  std::string device_id_;

  mojo::Receiver<cros::mojom::Camera3CallbackOps> callback_ops_;

  std::unique_ptr<StreamCaptureInterface> capture_interface_;

  raw_ptr<CameraDeviceContext> device_context_;

  bool zero_shutter_lag_supported_;

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
  std::unordered_set<raw_ptr<ResultMetadataObserver, CtnExperimental>>
      result_metadata_observers_;

  // The list of settings to set/override once in the capture request.
  std::vector<cros::mojom::CameraMetadataEntryPtr> capture_settings_override_;

  // The settings to set/override repeatedly in the capture request.  In
  // conflict with |capture_settings_override_|, this one has lower priority.
  std::map<cros::mojom::CameraMetadataTag, cros::mojom::CameraMetadataEntryPtr>
      capture_settings_repeating_override_;

  // Stores the pending capture results of the current in-flight frames.
  std::map<uint32_t, CaptureResult> pending_results_;

  std::queue<cros::mojom::CameraMetadataPtr> take_photo_settings_queue_;

  // Callback for TakePhoto(). When preparing capture request, the callback will
  // be popped and moved to CaptureResult.
  std::queue<VideoCaptureDevice::TakePhotoCallback> take_photo_callback_queue_;

  // Map for Portrait Mode request callbacks.
  TakePhotoCallbackMap take_portrait_photo_callback_map_;

  // Map for retrieving the last received frame number. It is used to check for
  // duplicate or out of order of frames.
  std::map<StreamType, uint32_t> last_received_frame_number_map_;

  // The JPEG thumbnail size chosen for current stream configuration.
  gfx::Size jpeg_thumbnail_size_;

  base::WeakPtr<CameraAppDeviceImpl> camera_app_device_;

  // The API version of the camera device.
  uint32_t device_api_version_;

  // Set true if the buffer management APIs are enabled.
  bool use_buffer_management_apis_;

  base::WeakPtrFactory<RequestManager> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_REQUEST_MANAGER_H_
