// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/image_processor/image_processor_client.h"

#include <functional>
#include <utility>

#include "base/functional/bind.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/image_processor_factory.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/test/image.h"
#include "media/gpu/test/video_frame_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace media {
namespace test {
namespace {

#define ASSERT_TRUE_OR_RETURN_NULLPTR(predicate) \
  do {                                           \
    if (!(predicate)) {                          \
      ADD_FAILURE();                             \
      return nullptr;                            \
    }                                            \
  } while (0)

std::optional<VideoFrameLayout> CreateLayout(
    const ImageProcessor::PortConfig& config) {
  const VideoPixelFormat pixel_format = config.fourcc.ToVideoPixelFormat();
  if (config.planes.empty())
    return std::nullopt;

  if (config.fourcc.IsMultiPlanar()) {
    return VideoFrameLayout::CreateWithPlanes(pixel_format, config.size,
                                              config.planes);
  } else {
    return VideoFrameLayout::CreateMultiPlanar(pixel_format, config.size,
                                               config.planes);
  }
}
}  // namespace

// static
std::unique_ptr<ImageProcessorClient> ImageProcessorClient::Create(
    std::optional<ImageProcessor::CreateBackendCB> create_backend_cb,
    const ImageProcessor::PortConfig& input_config,
    const ImageProcessor::PortConfig& output_config,
    size_t num_buffers,
    std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors) {
  auto ip_client =
      base::WrapUnique(new ImageProcessorClient(std::move(frame_processors)));
  if (!ip_client->CreateImageProcessor(create_backend_cb, input_config,
                                       output_config, num_buffers)) {
    LOG(ERROR) << "Failed to create ImageProcessor";
    return nullptr;
  }
  return ip_client;
}

ImageProcessorClient::ImageProcessorClient(
    std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors)
    : gpu_memory_buffer_factory_(
          gpu::GpuMemoryBufferFactory::CreateNativeType(nullptr)),
      frame_processors_(std::move(frame_processors)),
      image_processor_client_thread_("ImageProcessorClientThread"),
      output_cv_(&output_lock_),
      num_processed_frames_(0),
      image_processor_error_count_(0) {
  CHECK(image_processor_client_thread_.Start());
  DETACH_FROM_THREAD(image_processor_client_thread_checker_);
}

ImageProcessorClient::~ImageProcessorClient() {
  DCHECK_CALLED_ON_VALID_THREAD(test_main_thread_checker_);
  CHECK(image_processor_client_thread_.IsRunning());
  // Destroys |image_processor_| on |image_processor_client_thread_|.
  image_processor_client_thread_.task_runner()->DeleteSoon(
      FROM_HERE, image_processor_.release());
  image_processor_client_thread_.Stop();
}

bool ImageProcessorClient::CreateImageProcessor(
    std::optional<ImageProcessor::CreateBackendCB> create_backend_cb,
    const ImageProcessor::PortConfig& input_config,
    const ImageProcessor::PortConfig& output_config,
    size_t num_buffers) {
  DCHECK_CALLED_ON_VALID_THREAD(test_main_thread_checker_);
  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  // base::Unretained(this) and std::cref() are safe here because |this|,
  // |input_config| and |output_config| must outlive because this task is
  // blocking.
  image_processor_client_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ImageProcessorClient::CreateImageProcessorTask,
                                base::Unretained(this), create_backend_cb,
                                std::cref(input_config),
                                std::cref(output_config), num_buffers, &done));
  done.Wait();
  if (!image_processor_) {
    LOG(ERROR) << "Failed to create ImageProcessor";
    return false;
  }
  return true;
}

void ImageProcessorClient::CreateImageProcessorTask(
    std::optional<ImageProcessor::CreateBackendCB> create_backend_cb,
    const ImageProcessor::PortConfig& input_config,
    const ImageProcessor::PortConfig& output_config,
    size_t num_buffers,
    base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_THREAD(image_processor_client_thread_checker_);
  // base::Unretained(this) for ErrorCB is safe here because the callback is
  // executed on |image_processor_client_thread_| which is owned by this class.
  ImageProcessor::ErrorCB error_cb = base::BindRepeating(
      &ImageProcessorClient::NotifyError, base::Unretained(this));

  if (create_backend_cb) {
    image_processor_ =
        ImageProcessor::Create(*create_backend_cb, input_config, output_config,
                               ImageProcessor::OutputMode::IMPORT, error_cb,
                               image_processor_client_thread_.task_runner());
  } else {
    image_processor_ = ImageProcessorFactory::Create(
        input_config, output_config, num_buffers, error_cb,
        image_processor_client_thread_.task_runner());
  }

  done->Signal();
}

scoped_refptr<VideoFrame> ImageProcessorClient::CreateInputFrame(
    const Image& input_image) const {
  DCHECK_CALLED_ON_VALID_THREAD(test_main_thread_checker_);
  ASSERT_TRUE_OR_RETURN_NULLPTR(image_processor_);
  ASSERT_TRUE_OR_RETURN_NULLPTR(input_image.IsLoaded());

  const ImageProcessor::PortConfig& input_config =
      image_processor_->input_config();
  const VideoFrame::StorageType input_storage_type = input_config.storage_type;
  std::optional<VideoFrameLayout> input_layout = CreateLayout(input_config);
  ASSERT_TRUE_OR_RETURN_NULLPTR(input_layout);

  if (VideoFrame::IsStorageTypeMappable(input_storage_type)) {
    return CloneVideoFrame(CreateVideoFrameFromImage(input_image).get(),
                           *input_layout, VideoFrame::STORAGE_OWNED_MEMORY);
  } else {
    ASSERT_TRUE_OR_RETURN_NULLPTR(
        input_storage_type == VideoFrame::STORAGE_DMABUFS ||
        input_storage_type == VideoFrame::STORAGE_GPU_MEMORY_BUFFER);
    // NV12 is the only format that can be allocated with
    // gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE. So
    // gfx::BufferUsage::GPU_READ_CPU_READ_WRITE is specified for other formats.
    gfx::BufferUsage dst_buffer_usage =
        (PIXEL_FORMAT_NV12 == input_image.PixelFormat())
            ? gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE
            : gfx::BufferUsage::GPU_READ_CPU_READ_WRITE;
    return CloneVideoFrame(CreateVideoFrameFromImage(input_image).get(),
                           *input_layout, input_storage_type, dst_buffer_usage);
  }
}

scoped_refptr<VideoFrame> ImageProcessorClient::CreateOutputFrame(
    const Image& output_image) const {
  DCHECK_CALLED_ON_VALID_THREAD(test_main_thread_checker_);
  ASSERT_TRUE_OR_RETURN_NULLPTR(output_image.IsMetadataLoaded());
  ASSERT_TRUE_OR_RETURN_NULLPTR(image_processor_);

  const ImageProcessor::PortConfig& output_config =
      image_processor_->output_config();
  const VideoFrame::StorageType output_storage_type =
      output_config.storage_type;
  std::optional<VideoFrameLayout> output_layout = CreateLayout(output_config);
  ASSERT_TRUE_OR_RETURN_NULLPTR(output_layout);
  if (VideoFrame::IsStorageTypeMappable(output_storage_type)) {
    return VideoFrame::CreateFrameWithLayout(
        *output_layout, gfx::Rect(output_image.Size()), output_image.Size(),
        base::TimeDelta(), false /* zero_initialize_memory*/);
  }

  ASSERT_TRUE_OR_RETURN_NULLPTR(
      output_storage_type == VideoFrame::STORAGE_DMABUFS ||
      output_storage_type == VideoFrame::STORAGE_GPU_MEMORY_BUFFER);
  scoped_refptr<VideoFrame> output_frame = CreatePlatformVideoFrame(
      output_layout->format(), output_layout->coded_size(),
      gfx::Rect(output_image.Size()), output_image.Size(), base::TimeDelta(),
      gfx::BufferUsage::GPU_READ_CPU_READ_WRITE);

  if (output_storage_type == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    output_frame = CreateGpuMemoryBufferVideoFrame(
        output_frame.get(), gfx::BufferUsage::GPU_READ_CPU_READ_WRITE);
  }
  return output_frame;
}

void ImageProcessorClient::FrameReady(size_t frame_index,
                                      scoped_refptr<VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_THREAD(image_processor_client_thread_checker_);

  base::AutoLock auto_lock_(output_lock_);
  // VideoFrame should be processed in FIFO order.
  EXPECT_EQ(frame_index, num_processed_frames_);
  for (auto& processor : frame_processors_)
    processor->ProcessVideoFrame(frame, frame_index);
  num_processed_frames_++;
  output_cv_.Signal();
}

bool ImageProcessorClient::WaitUntilNumImageProcessed(
    size_t num_processed,
    base::TimeDelta max_wait) {
  base::TimeDelta time_waiting;
  // NOTE: Acquire lock here does not matter, because
  // base::ConditionVariable::TimedWait() unlocks output_lock_ at the start and
  // locks again at the end.
  base::AutoLock auto_lock_(output_lock_);
  while (time_waiting < max_wait) {
    if (num_processed_frames_ >= num_processed)
      return true;

    const base::TimeTicks start_time = base::TimeTicks::Now();
    output_cv_.TimedWait(max_wait);
    time_waiting += base::TimeTicks::Now() - start_time;
  }

  return false;
}

bool ImageProcessorClient::WaitForFrameProcessors() {
  bool success = true;
  for (auto& processor : frame_processors_)
    success &= processor->WaitUntilDone();

  return success;
}

size_t ImageProcessorClient::GetNumOfProcessedImages() const {
  base::AutoLock auto_lock_(output_lock_);
  return num_processed_frames_;
}

size_t ImageProcessorClient::GetErrorCount() const {
  base::AutoLock auto_lock_(output_lock_);
  return image_processor_error_count_;
}

void ImageProcessorClient::NotifyError() {
  DCHECK_CALLED_ON_VALID_THREAD(image_processor_client_thread_checker_);
  base::AutoLock auto_lock_(output_lock_);
  image_processor_error_count_++;
}

void ImageProcessorClient::Process(const Image& input_image,
                                   const Image& output_image) {
  DCHECK_CALLED_ON_VALID_THREAD(test_main_thread_checker_);
  auto input_frame = CreateInputFrame(input_image);
  ASSERT_TRUE(input_frame);
  auto output_frame = CreateOutputFrame(output_image);
  ASSERT_TRUE(output_frame);
  image_processor_client_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ImageProcessorClient::ProcessTask, base::Unretained(this),
                     std::move(input_frame), std::move(output_frame)));
}

void ImageProcessorClient::ProcessTask(scoped_refptr<VideoFrame> input_frame,
                                       scoped_refptr<VideoFrame> output_frame) {
  DCHECK_CALLED_ON_VALID_THREAD(image_processor_client_thread_checker_);
  // base::Unretained(this) and std::cref() for FrameReadyCB is safe here
  // because the callback is executed on |image_processor_client_thread_| which
  // is owned by this class.
  image_processor_->Process(std::move(input_frame), std::move(output_frame),
                            base::BindPostTaskToCurrentDefault(base::BindOnce(
                                &ImageProcessorClient::FrameReady,
                                base::Unretained(this), next_frame_index_)));
  next_frame_index_++;
}

}  // namespace test
}  // namespace media
