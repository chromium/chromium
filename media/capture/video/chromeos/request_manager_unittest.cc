// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/video/chromeos/request_manager.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "media/capture/video/blob_utils.h"
#include "media/capture/video/chromeos/camera_buffer_factory.h"
#include "media/capture/video/chromeos/camera_device_context.h"
#include "media/capture/video/chromeos/camera_device_delegate.h"
#include "media/capture/video/chromeos/mock_video_capture_client.h"
#include "media/capture/video/chromeos/stream_buffer_manager.h"
#include "media/capture/video/mock_gpu_memory_buffer_manager.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::A;
using testing::AtLeast;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Return;

namespace media {

namespace {

class MockStreamCaptureInterface : public StreamCaptureInterface {
 public:
  void ProcessCaptureRequest(
      cros::mojom::Camera3CaptureRequestPtr request,
      base::OnceCallback<void(int32_t)> callback) override {
    DoProcessCaptureRequest(request, callback);
  }
  MOCK_METHOD2(DoProcessCaptureRequest,
               void(cros::mojom::Camera3CaptureRequestPtr& request,
                    base::OnceCallback<void(int32_t)>& callback));

  void OnNewBuffer(ClientType client_type,
                   cros::mojom::CameraBufferHandlePtr buffer) override {
    DoOnNewBuffer(client_type, std::move(buffer));
  }
  MOCK_METHOD2(DoOnNewBuffer,
               void(ClientType client_type,
                    cros::mojom::CameraBufferHandlePtr buffer));

  void OnBufferRetired(ClientType client_type, uint64_t buffer_id) override {
    DoOnBufferRetired(client_type, buffer_id);
  }
  MOCK_METHOD2(DoOnBufferRetired,
               void(ClientType client_type, uint64_t buffer_id));

  void Flush(base::OnceCallback<void(int32_t)> callback) override {
    DoFlush(callback);
  }
  MOCK_METHOD1(DoFlush, void(base::OnceCallback<void(int32_t)>& callback));
};

const VideoCaptureFormat kDefaultCaptureFormat(gfx::Size(1280, 720),
                                               30.0,
                                               PIXEL_FORMAT_NV12);

class FakeCameraBufferFactory : public CameraBufferFactory {
 public:
  FakeCameraBufferFactory() {
    gpu_memory_buffer_manager_ =
        std::make_unique<unittest_internal::MockGpuMemoryBufferManager>();
  }
  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override {
    return unittest_internal::MockGpuMemoryBufferManager::
        CreateFakeGpuMemoryBuffer(size, format, usage, gpu::kNullSurfaceHandle,
                                  nullptr);
  }

  ChromiumPixelFormat ResolveStreamBufferFormat(
      cros::mojom::HalPixelFormat hal_format,
      gfx::BufferUsage usage) override {
    return ChromiumPixelFormat{PIXEL_FORMAT_NV12,
                               gfx::BufferFormat::YUV_420_BIPLANAR};
  }

 private:
  std::unique_ptr<unittest_internal::MockGpuMemoryBufferManager>
      gpu_memory_buffer_manager_;
};

}  // namespace

class RequestManagerTest : public ::testing::TestWithParam<bool> {
 public:
  enum class NotifyAndProcessCaptureResultMode {
    Default,
    PartialResult,
    ResultError
  };

  void SetUp() override {
    quit_ = false;
    client_type_ = ClientType::kPreviewClient;
    VideoCaptureParams params;
    params.requested_format = kDefaultCaptureFormat;
    capture_params_[client_type_] = params;
    device_context_ = std::make_unique<CameraDeviceContext>();
    use_buffer_management_apis_ = GetParam();
    if (device_context_->AddClient(
            client_type_,
            std::make_unique<unittest_internal::MockVideoCaptureClient>())) {
      std::string fake_device_id = "0";
      request_manager_ = std::make_unique<RequestManager>(
          fake_device_id, mock_callback_ops_.BindNewPipeAndPassReceiver(),
          std::make_unique<MockStreamCaptureInterface>(), device_context_.get(),
          VideoCaptureBufferType::kSharedMemory,
          std::make_unique<FakeCameraBufferFactory>(),
          base::BindRepeating(
              [](const uint8_t* buffer, const uint32_t bytesused,
                 const VideoCaptureFormat& capture_format,
                 const int rotation) { return mojom::Blob::New(); }),
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          use_buffer_management_apis_
              ? cros::mojom::CAMERA_DEVICE_API_VERSION_3_6
              : cros::mojom::CAMERA_DEVICE_API_VERSION_3_5,
          use_buffer_management_apis_);
    }
  }

  void TearDown() override {
    device_context_->RemoveClient(client_type_);
    request_manager_.reset();
  }

  void DoLoop() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  void QuitCaptureLoop() {
    quit_ = true;
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  cros::mojom::CameraMetadataPtr GetFakeStaticMetadata(
      int32_t partial_result_count) {
    cros::mojom::CameraMetadataPtr static_metadata =
        cros::mojom::CameraMetadata::New();
    static_metadata->entry_count = 2;
    static_metadata->entry_capacity = 2;
    static_metadata->entries =
        std::vector<cros::mojom::CameraMetadataEntryPtr>();

    cros::mojom::CameraMetadataEntryPtr entry =
        cros::mojom::CameraMetadataEntry::New();
    entry->index = 0;
    entry->tag =
        cros::mojom::CameraMetadataTag::ANDROID_REQUEST_PARTIAL_RESULT_COUNT;
    entry->type = cros::mojom::EntryType::TYPE_INT32;
    entry->count = 1;
    uint8_t* as_int8 = reinterpret_cast<uint8_t*>(&partial_result_count);
    entry->data.assign(as_int8, as_int8 + entry->count * sizeof(int32_t));
    static_metadata->entries->push_back(std::move(entry));

    entry = cros::mojom::CameraMetadataEntry::New();
    entry->index = 1;
    entry->tag = cros::mojom::CameraMetadataTag::ANDROID_JPEG_MAX_SIZE;
    entry->type = cros::mojom::EntryType::TYPE_INT32;
    entry->count = 1;
    int32_t jpeg_max_size = 65535;
    as_int8 = reinterpret_cast<uint8_t*>(&jpeg_max_size);
    entry->data.assign(as_int8, as_int8 + entry->count * sizeof(int32_t));
    static_metadata->entries->push_back(std::move(entry));

    entry = cros::mojom::CameraMetadataEntry::New();
    entry->index = 2;
    entry->tag =
        cros::mojom::CameraMetadataTag::ANDROID_REQUEST_PIPELINE_MAX_DEPTH;
    entry->type = cros::mojom::EntryType::TYPE_BYTE;
    entry->count = 1;
    uint8_t pipeline_max_depth = 1;
    entry->data.assign(&pipeline_max_depth,
                       &pipeline_max_depth + entry->count * sizeof(uint8_t));
    static_metadata->entries->push_back(std::move(entry));

    return static_metadata;
  }

  void ProcessCaptureRequestDefault(
      cros::mojom::Camera3CaptureRequestPtr& request,
      base::OnceCallback<void(int32_t)>& callback) {
    ProcessCaptureRequest(NotifyAndProcessCaptureResultMode::Default, request,
                          callback);
  }

  void ProcessCaptureRequestPartialResult(
      cros::mojom::Camera3CaptureRequestPtr& request,
      base::OnceCallback<void(int32_t)>& callback) {
    ProcessCaptureRequest(NotifyAndProcessCaptureResultMode::PartialResult,
                          request, callback);
  }

  void ProcessCaptureRequestResultError(
      cros::mojom::Camera3CaptureRequestPtr& request,
      base::OnceCallback<void(int32_t)>& callback) {
    ProcessCaptureRequest(NotifyAndProcessCaptureResultMode::ResultError,
                          request, callback);
  }

  void ProcessCaptureRequest(NotifyAndProcessCaptureResultMode mode,
                             cros::mojom::Camera3CaptureRequestPtr& request,
                             base::OnceCallback<void(int32_t)>& callback) {
    if (quit_) {
      return;
    }
    std::move(callback).Run(0);

    if (!use_buffer_management_apis_) {
      NotifyAndProcessCaptureResult(mode, request->frame_number,
                                    std::move(request->output_buffers));
      return;
    }

    mock_callback_ops_->RequestStreamBuffers(
        PrepareBufferRequest(request),
        base::BindOnce(&RequestManagerTest::RequestStreamBuffersCallback,
                       base::Unretained(this), mode, request->frame_number));
  }

  void RequestStreamBuffersCallback(
      NotifyAndProcessCaptureResultMode mode,
      uint32_t frame_number,
      cros::mojom::Camera3BufferRequestStatus status,
      std::vector<cros::mojom::Camera3StreamBufferRetPtr> buffers) {
    if (quit_) {
      return;
    }
    std::vector<cros::mojom::Camera3StreamBufferPtr> output_buffers;
    for (const auto& buffer : buffers) {
      output_buffers.push_back(std::move((*buffer->output_buffers)[0]));
    }
    NotifyAndProcessCaptureResult(mode, frame_number,
                                  std::move(output_buffers));
  }

  void NotifyAndProcessCaptureResult(
      NotifyAndProcessCaptureResultMode mode,
      uint32_t frame_number,
      std::vector<cros::mojom::Camera3StreamBufferPtr> output_buffers) {
    switch (mode) {
      case NotifyAndProcessCaptureResultMode::Default:
        mock_callback_ops_->Notify(PrepareShutterNotifyMessage(
            frame_number,
            (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds()));
        mock_callback_ops_->ProcessCaptureResult(PrepareCapturedResult(
            frame_number, cros::mojom::CameraMetadata::New(), 1,
            std::move(output_buffers)));
        break;

      case NotifyAndProcessCaptureResultMode::PartialResult:
        mock_callback_ops_->Notify(PrepareShutterNotifyMessage(
            frame_number,
            (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds()));
        mock_callback_ops_->ProcessCaptureResult(PrepareCapturedResult(
            frame_number, cros::mojom::CameraMetadata::New(), 1,
            std::move(output_buffers)));
        mock_callback_ops_->ProcessCaptureResult(PrepareCapturedResult(
            frame_number, cros::mojom::CameraMetadata::New(), 2,
            std::vector<cros::mojom::Camera3StreamBufferPtr>()));
        mock_callback_ops_->ProcessCaptureResult(PrepareCapturedResult(
            frame_number, cros::mojom::CameraMetadata::New(), 3,
            std::vector<cros::mojom::Camera3StreamBufferPtr>()));
        break;

      case NotifyAndProcessCaptureResultMode::ResultError:
        mock_callback_ops_->Notify(PrepareShutterNotifyMessage(
            frame_number,
            (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds()));
        mock_callback_ops_->ProcessCaptureResult(PrepareCapturedResult(
            frame_number, cros::mojom::CameraMetadata::New(), 1,
            std::move(output_buffers)));
        // Send a result error notify without sending the second partial result.
        // RequestManager should submit the buffer when it receives the
        // result error.
        mock_callback_ops_->Notify(PrepareErrorNotifyMessage(
            frame_number,
            cros::mojom::Camera3ErrorMsgCode::CAMERA3_MSG_ERROR_RESULT));
        break;
    }
  }

  MockStreamCaptureInterface* GetMockCaptureInterface() {
    EXPECT_NE(nullptr, request_manager_.get());
    return reinterpret_cast<MockStreamCaptureInterface*>(
        request_manager_->capture_interface_.get());
  }

  unittest_internal::MockVideoCaptureClient* GetMockVideoCaptureClient() {
    EXPECT_NE(nullptr, device_context_);
    base::AutoLock lock(device_context_->client_lock_);
    EXPECT_TRUE(!device_context_->clients_.empty());
    return reinterpret_cast<unittest_internal::MockVideoCaptureClient*>(
        device_context_->clients_[client_type_].get());
  }

  std::map<uint32_t, RequestManager::CaptureResult>& GetPendingResults() {
    EXPECT_NE(nullptr, request_manager_.get());
    return request_manager_->pending_results_;
  }

  std::vector<cros::mojom::Camera3StreamPtr> PrepareCaptureStream(
      uint32_t max_buffers) {
    std::vector<cros::mojom::Camera3StreamPtr> streams;

    auto preview_stream = cros::mojom::Camera3Stream::New();
    preview_stream->id = static_cast<uint64_t>(StreamType::kPreviewOutput);
    preview_stream->stream_type =
        cros::mojom::Camera3StreamType::CAMERA3_STREAM_OUTPUT;
    preview_stream->width = kDefaultCaptureFormat.frame_size.width();
    preview_stream->height = kDefaultCaptureFormat.frame_size.height();
    preview_stream->format =
        cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YCbCr_420_888;
    preview_stream->usage = 0;
    preview_stream->max_buffers = max_buffers;
    preview_stream->data_space = 0;
    preview_stream->rotation =
        cros::mojom::Camera3StreamRotation::CAMERA3_STREAM_ROTATION_0;
    streams.push_back(std::move(preview_stream));

    auto still_capture_stream = cros::mojom::Camera3Stream::New();
    still_capture_stream->id = static_cast<uint64_t>(StreamType::kJpegOutput);
    still_capture_stream->stream_type =
        cros::mojom::Camera3StreamType::CAMERA3_STREAM_OUTPUT;
    still_capture_stream->width = kDefaultCaptureFormat.frame_size.width();
    still_capture_stream->height = kDefaultCaptureFormat.frame_size.height();
    still_capture_stream->format =
        cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_BLOB;
    still_capture_stream->usage = 0;
    still_capture_stream->max_buffers = max_buffers;
    still_capture_stream->data_space = 0;
    still_capture_stream->rotation =
        cros::mojom::Camera3StreamRotation::CAMERA3_STREAM_ROTATION_0;
    streams.push_back(std::move(still_capture_stream));

    return streams;
  }

  cros::mojom::Camera3NotifyMsgPtr PrepareErrorNotifyMessage(
      uint32_t frame_number,
      cros::mojom::Camera3ErrorMsgCode error_code) {
    auto error_msg = cros::mojom::Camera3ErrorMsg::New();
    error_msg->frame_number = frame_number;
    // There is only the preview stream.
    error_msg->error_stream_id =
        static_cast<uint64_t>(StreamType::kPreviewOutput);
    error_msg->error_code = error_code;
    auto notify_msg = cros::mojom::Camera3NotifyMsg::New();
    notify_msg->type = cros::mojom::Camera3MsgType::CAMERA3_MSG_ERROR;
    notify_msg->message =
        cros::mojom::Camera3NotifyMsgMessage::NewError(std::move(error_msg));
    return notify_msg;
  }

  cros::mojom::Camera3NotifyMsgPtr PrepareShutterNotifyMessage(
      uint32_t frame_number,
      uint64_t timestamp) {
    auto shutter_msg = cros::mojom::Camera3ShutterMsg::New();
    shutter_msg->frame_number = frame_number;
    shutter_msg->timestamp = timestamp;
    auto notify_msg = cros::mojom::Camera3NotifyMsg::New();
    notify_msg->type = cros::mojom::Camera3MsgType::CAMERA3_MSG_SHUTTER;
    notify_msg->message = cros::mojom::Camera3NotifyMsgMessage::NewShutter(
        std::move(shutter_msg));
    return notify_msg;
  }

  cros::mojom::Camera3CaptureResultPtr PrepareCapturedResult(
      uint32_t frame_number,
      cros::mojom::CameraMetadataPtr result_metadata,
      uint32_t partial_result,
      std::vector<cros::mojom::Camera3StreamBufferPtr> output_buffers) {
    auto result = cros::mojom::Camera3CaptureResult::New();
    result->frame_number = frame_number;
    result->result = std::move(result_metadata);
    if (output_buffers.size()) {
      result->output_buffers = std::move(output_buffers);
    }
    result->partial_result = partial_result;
    return result;
  }

  std::vector<cros::mojom::Camera3BufferRequestPtr> PrepareBufferRequest(
      const cros::mojom::Camera3CaptureRequestPtr& request) {
    std::vector<cros::mojom::Camera3BufferRequestPtr> reqs;
    for (const auto& output_buffer : request->output_buffers) {
      auto req = cros::mojom::Camera3BufferRequest::New();
      req->stream_id = output_buffer->stream_id;
      req->num_buffers_requested = 1;
      reqs.push_back(std::move(req));
    }
    return reqs;
  }

 protected:
  std::unique_ptr<RequestManager> request_manager_;
  mojo::Remote<cros::mojom::Camera3CallbackOps> mock_callback_ops_;
  std::unique_ptr<CameraDeviceContext> device_context_;
  ClientType client_type_;
  base::flat_map<ClientType, VideoCaptureParams> capture_params_;

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  bool quit_;
  bool use_buffer_management_apis_;
  base::test::TaskEnvironment scoped_test_environment_;
};

// A basic sanity test to capture one frame with the capture loop.
TEST_P(RequestManagerTest, SimpleCaptureTest) {
  GetMockVideoCaptureClient()->SetFrameCb(base::BindOnce(
      &RequestManagerTest::QuitCaptureLoop, base::Unretained(this)));
  EXPECT_CALL(*GetMockCaptureInterface(), DoProcessCaptureRequest(_, _))
      .Times(AtLeast(1))
      .WillRepeatedly(
          Invoke(this, &RequestManagerTest::ProcessCaptureRequestDefault));

  request_manager_->SetUpStreamsAndBuffers(
      capture_params_, GetFakeStaticMetadata(/* partial_result_count */ 1),
      PrepareCaptureStream(/* max_buffers */ 1));
  request_manager_->StartPreview(cros::mojom::CameraMetadata::New());

  // Wait until a captured frame is received by MockVideoCaptureClient.
  DoLoop();
}

// Test that the RequestManager submits a captured result only after all
// partial metadata are received.
TEST_P(RequestManagerTest, PartialResultTest) {
  GetMockVideoCaptureClient()->SetFrameCb(base::BindOnce(
      [](RequestManagerTest* test) {
        EXPECT_EQ(1u, test->GetPendingResults().size());
        // Make sure all the three partial metadata are received before the
        // captured result is submitted.
        EXPECT_EQ(
            3u, test->GetPendingResults()[0].partial_metadata_received.size());
        test->QuitCaptureLoop();
      },
      base::Unretained(this)));
  EXPECT_CALL(*GetMockCaptureInterface(), DoProcessCaptureRequest(_, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(
          this, &RequestManagerTest::ProcessCaptureRequestPartialResult));

  request_manager_->SetUpStreamsAndBuffers(
      capture_params_, GetFakeStaticMetadata(/* partial_result_count */ 3),
      PrepareCaptureStream(/* max_buffers */ 1));
  request_manager_->StartPreview(cros::mojom::CameraMetadata::New());

  // Wait until a captured frame is received by MockVideoCaptureClient.
  DoLoop();
}

// Test that the capture loop is stopped and no frame is submitted when a device
// error happens.
TEST_P(RequestManagerTest, DeviceErrorTest) {
  GetMockVideoCaptureClient()->SetFrameCb(base::BindOnce(
      [](RequestManagerTest* test) {
        ADD_FAILURE() << "No frame should be submitted";
        test->QuitCaptureLoop();
      },
      base::Unretained(this)));
  EXPECT_CALL(*GetMockVideoCaptureClient(), OnError(_, _, _))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(this, &RequestManagerTest::QuitCaptureLoop));
  EXPECT_CALL(*GetMockCaptureInterface(), DoProcessCaptureRequest(_, _))
      .Times(1)
      .WillOnce(Invoke([this](cros::mojom::Camera3CaptureRequestPtr& request,
                              base::OnceCallback<void(int32_t)>& callback) {
        std::move(callback).Run(0);
        mock_callback_ops_->Notify(PrepareErrorNotifyMessage(
            request->frame_number,
            cros::mojom::Camera3ErrorMsgCode::CAMERA3_MSG_ERROR_DEVICE));
      }));

  request_manager_->SetUpStreamsAndBuffers(
      capture_params_, GetFakeStaticMetadata(/* partial_result_count */ 1),
      PrepareCaptureStream(/* max_buffers */ 1));
  request_manager_->StartPreview(cros::mojom::CameraMetadata::New());

  // Wait until the MockVideoCaptureClient is deleted.
  DoLoop();
}

// Test that upon request error the erroneous frame is dropped, and the capture
// loop continues.
TEST_P(RequestManagerTest, RequestErrorTest) {
  GetMockVideoCaptureClient()->SetFrameCb(base::BindOnce(
      [](RequestManagerTest* test) {
        // Frame 0 should be dropped, and the frame callback should be called
        // with frame 1.
        EXPECT_EQ(test->GetPendingResults().end(),
                  test->GetPendingResults().find(0));
        EXPECT_NE(test->GetPendingResults().end(),
                  test->GetPendingResults().find(1));
        test->QuitCaptureLoop();
      },
      base::Unretained(this)));
  EXPECT_CALL(*GetMockCaptureInterface(), DoProcessCaptureRequest(_, _))
      .Times(AtLeast(2))
      .WillOnce(Invoke([this](cros::mojom::Camera3CaptureRequestPtr& request,
                              base::OnceCallback<void(int32_t)>& callback) {
        std::move(callback).Run(0);
        mock_callback_ops_->Notify(PrepareErrorNotifyMessage(
            request->frame_number,
            cros::mojom::Camera3ErrorMsgCode::CAMERA3_MSG_ERROR_REQUEST));
        request->output_buffers[0]->status =
            cros::mojom::Camera3BufferStatus::CAMERA3_BUFFER_STATUS_ERROR;
        mock_callback_ops_->ProcessCaptureResult(PrepareCapturedResult(
            request->frame_number, cros::mojom::CameraMetadata::New(), 1,
            std::move(request->output_buffers)));
      }))
      .WillRepeatedly(
          Invoke(this, &RequestManagerTest::ProcessCaptureRequestDefault));

  request_manager_->SetUpStreamsAndBuffers(
      capture_params_, GetFakeStaticMetadata(/* partial_result_count */ 1),
      PrepareCaptureStream(/* max_buffers */ 1));
  request_manager_->StartPreview(cros::mojom::CameraMetadata::New());

  // Wait until the MockVideoCaptureClient is deleted.
  DoLoop();
}

// Test that upon result error the captured buffer is submitted despite of the
// missing result metadata, and the capture loop continues.
TEST_P(RequestManagerTest, ResultErrorTest) {
  GetMockVideoCaptureClient()->SetFrameCb(base::BindOnce(
      [](RequestManagerTest* test) {
        // Frame 0 should be submitted.
        EXPECT_NE(test->GetPendingResults().end(),
                  test->GetPendingResults().find(0));
        test->QuitCaptureLoop();
      },
      base::Unretained(this)));
  EXPECT_CALL(*GetMockCaptureInterface(), DoProcessCaptureRequest(_, _))
      .Times(AtLeast(1))
      .WillOnce(
          Invoke(this, &RequestManagerTest::ProcessCaptureRequestResultError))
      .WillRepeatedly(
          Invoke(this, &RequestManagerTest::ProcessCaptureRequestDefault));

  request_manager_->SetUpStreamsAndBuffers(
      capture_params_, GetFakeStaticMetadata(/* partial_result_count */ 2),
      PrepareCaptureStream(/* max_buffers */ 1));
  request_manager_->StartPreview(cros::mojom::CameraMetadata::New());

  // Wait until the MockVideoCaptureClient is deleted.
  DoLoop();
}

// Test that upon buffer error the erroneous buffer is dropped, and the capture
// loop continues.
TEST_P(RequestManagerTest, BufferErrorTest) {
  GetMockVideoCaptureClient()->SetFrameCb(base::BindOnce(
      [](RequestManagerTest* test) {
        // Frame 0 should be dropped, and the frame callback should be called
        // with frame 1.
        EXPECT_EQ(test->GetPendingResults().end(),
                  test->GetPendingResults().find(0));
        EXPECT_NE(test->GetPendingResults().end(),
                  test->GetPendingResults().find(1));
        test->QuitCaptureLoop();
      },
      base::Unretained(this)));
  EXPECT_CALL(*GetMockCaptureInterface(), DoProcessCaptureRequest(_, _))
      .Times(AtLeast(2))
      .WillOnce(Invoke([this](cros::mojom::Camera3CaptureRequestPtr& request,
                              base::OnceCallback<void(int32_t)>& callback) {
        std::move(callback).Run(0);
        mock_callback_ops_->Notify(PrepareShutterNotifyMessage(
            request->frame_number,
            (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds()));
        mock_callback_ops_->Notify(PrepareErrorNotifyMessage(
            request->frame_number,
            cros::mojom::Camera3ErrorMsgCode::CAMERA3_MSG_ERROR_BUFFER));
        request->output_buffers[0]->status =
            cros::mojom::Camera3BufferStatus::CAMERA3_BUFFER_STATUS_ERROR;
        mock_callback_ops_->ProcessCaptureResult(PrepareCapturedResult(
            request->frame_number, cros::mojom::CameraMetadata::New(), 1,
            std::move(request->output_buffers)));
      }))
      .WillRepeatedly(
          Invoke(this, &RequestManagerTest::ProcessCaptureRequestDefault));

  request_manager_->SetUpStreamsAndBuffers(
      capture_params_, GetFakeStaticMetadata(/* partial_result_count */ 1),
      PrepareCaptureStream(/* max_buffers */ 1));
  request_manager_->StartPreview(cros::mojom::CameraMetadata::New());

  // Wait until the MockVideoCaptureClient is deleted.
  DoLoop();
}

INSTANTIATE_TEST_SUITE_P(, RequestManagerTest, ::testing::Bool());

// Test that preview and still capture buffers can be correctly submitted.
// TODO(crbug.com/40607619): Add take photo test.

}  // namespace media
