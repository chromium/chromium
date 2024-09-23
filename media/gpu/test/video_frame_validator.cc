// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/test/video_frame_validator.h"

#include <string_view>

#include "base/containers/span.h"
#include "base/cpu.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/hash/md5.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/macros.h"
#include "media/gpu/test/image_quality_metrics.h"
#include "media/gpu/test/video_frame_helpers.h"
#include "media/gpu/test/video_test_helpers.h"
#include "media/gpu/video_frame_mapper.h"
#include "media/gpu/video_frame_mapper_factory.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#include <sys/mman.h>
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

namespace media {
namespace test {

VideoFrameValidator::VideoFrameValidator(
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor,
    CropHelper crop_helper)
    : corrupt_frame_processor_(std::move(corrupt_frame_processor)),
      crop_helper_(crop_helper),
      num_frames_validating_(0),
      frame_validator_thread_("FrameValidatorThread"),
      frame_validator_cv_(&frame_validator_lock_) {
  DETACH_FROM_SEQUENCE(validator_sequence_checker_);
  DETACH_FROM_SEQUENCE(validator_thread_sequence_checker_);
}

VideoFrameValidator::~VideoFrameValidator() {
  Destroy();
}

bool VideoFrameValidator::Initialize() {
  if (!frame_validator_thread_.Start()) {
    LOG(ERROR) << "Failed to start frame validator thread";
    return false;
  }
  return true;
}

void VideoFrameValidator::CleanUpOnValidatorThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);
  video_frame_mapper_.reset();
}

void VideoFrameValidator::Destroy() {
  if (frame_validator_thread_.task_runner()) {
    // It's safe to use base::Unretained(this) because we own
    // |frame_validator_thread_|, so |this| should be valid until at least the
    // frame_validator_thread_.Stop() returns below which won't happen until
    // CleanUpOnValidatorThread() returns.
    frame_validator_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoFrameValidator::CleanUpOnValidatorThread,
                       base::Unretained(this)));
  }
  frame_validator_thread_.Stop();
  base::AutoLock auto_lock(frame_validator_lock_);
  DCHECK_EQ(0u, num_frames_validating_);

  corrupt_frame_processor_.reset();
}

void VideoFrameValidator::PrintMismatchedFramesInfo() const {
  base::AutoLock auto_lock(frame_validator_lock_);
  for (const auto& mismatched_frame_info : mismatched_frames_)
    mismatched_frame_info->Print();
}

size_t VideoFrameValidator::GetMismatchedFramesCount() const {
  base::AutoLock auto_lock(frame_validator_lock_);
  return mismatched_frames_.size();
}

void VideoFrameValidator::ProcessVideoFrame(
    scoped_refptr<const VideoFrame> video_frame,
    size_t frame_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_sequence_checker_);

  if (!video_frame) {
    LOG(ERROR) << "Video frame is nullptr";
    return;
  }

  if (video_frame->visible_rect().IsEmpty()) {
    // This occurs in bitstream buffer in webrtc scenario.
    DLOG(WARNING) << "Skipping validation, frame_index=" << frame_index
                  << " because visible_rect is empty";
    return;
  }

  base::AutoLock auto_lock(frame_validator_lock_);
  num_frames_validating_++;

  // Unretained is safe here, as we should not destroy the validator while there
  // are still frames being validated.
  frame_validator_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameValidator::ProcessVideoFrameTask,
                     base::Unretained(this), video_frame, frame_index));
}

bool VideoFrameValidator::WaitUntilDone() {
  {
    base::AutoLock auto_lock(frame_validator_lock_);
    while (num_frames_validating_ > 0) {
      frame_validator_cv_.Wait();
    }

    if (corrupt_frame_processor_ && !corrupt_frame_processor_->WaitUntilDone())
      return false;
  }

  if (!Passed()) {
    LOG(ERROR) << GetMismatchedFramesCount() << " frames failed to validate.";
    PrintMismatchedFramesInfo();
    return false;
  }
  return true;
}

bool VideoFrameValidator::Passed() const {
  return GetMismatchedFramesCount() == 0u;
}

void VideoFrameValidator::ProcessVideoFrameTask(
    const scoped_refptr<const VideoFrame> video_frame,
    size_t frame_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);

  scoped_refptr<const VideoFrame> frame = video_frame;
  // If this is a DMABuf-backed memory frame we need to map it before accessing.
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  if (frame->storage_type() == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    // TODO(andrescj): This is a workaround. ClientNativePixmapFactoryDmabuf
    // creates ClientNativePixmapOpaque for SCANOUT_VDA_WRITE buffers which
    // does not allow us to map GpuMemoryBuffers easily for testing.
    // Therefore, we extract the dma-buf FDs. Alternatively, we could consider
    // creating our own ClientNativePixmapFactory for testing.
    frame = CreateDmabufVideoFrame(frame.get());
    if (!frame) {
      LOG(ERROR) << "Failed to create Dmabuf-backed VideoFrame from "
                 << "GpuMemoryBuffer-based VideoFrame";
      return;
    }
  }

  if (frame->storage_type() == VideoFrame::STORAGE_DMABUFS) {
    // Create VideoFrameMapper if not yet created. The decoder's output pixel
    // format is not known yet when creating the VideoFrameValidator. We can
    // only create the VideoFrameMapper upon receiving the first video frame.
    if (!video_frame_mapper_) {
      video_frame_mapper_ = VideoFrameMapperFactory::CreateMapper(
          frame->format(), frame->storage_type());
      ASSERT_TRUE(video_frame_mapper_) << "Failed to create VideoFrameMapper";
    }

    frame = video_frame_mapper_->Map(std::move(frame), PROT_READ);
    if (!frame) {
      LOG(ERROR) << "Failed to map video frame";
      return;
    }
  }
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

  ASSERT_TRUE(frame->IsMappable());

  if (ShouldCrop())
    frame = CloneAndCropFrame(std::move(frame));

  auto mismatched_info = Validate(frame, frame_index);

  base::AutoLock auto_lock(frame_validator_lock_);
  if (mismatched_info) {
    mismatched_frames_.push_back(std::move(mismatched_info));
    // Perform additional processing on the corrupt video frame if requested.
    // Pass |video_frame|, not |frame|, so that |frame| is destroyed in
    // |frame_validator_thread_|.
    if (corrupt_frame_processor_)
      corrupt_frame_processor_->ProcessVideoFrame(video_frame, frame_index);
  }

  num_frames_validating_--;
  frame_validator_cv_.Signal();
}

scoped_refptr<VideoFrame> VideoFrameValidator::CloneAndCropFrame(
    scoped_refptr<const VideoFrame> frame) const {
  const auto crop = crop_helper_.Run(*frame);
  const auto& visible = frame->visible_rect();
  auto cloned_frame = CloneVideoFrame(frame.get(), frame->layout());
  // Ensures that the crop is within the previous visible rectangle.
  if (!visible.Contains(crop)) {
    LOG(ERROR) << "Crop " << crop.ToString()
               << "must be contained by the visible area of the input frame: "
               << visible.ToString() << ".";
    return cloned_frame;
  }
  return VideoFrame::WrapVideoFrame(cloned_frame, frame->format(), crop,
                                    crop.size());
}

gfx::Rect BottomRowCrop(int row_height, const VideoFrame& frame) {
  const auto& visible = frame.visible_rect();
  // Performs bounds checking on the row_height to ensure that the returned
  // crop is contained by the current visible area.
  if (visible.size().height() < row_height) {
    LOG(ERROR) << "Could not get a row of height " << row_height
               << " from a visible area of height " << visible.size().height();
    return visible;
  }

  return gfx::Rect{visible.x(), visible.bottom() - row_height, visible.width(),
                   row_height};
}

struct MD5VideoFrameValidator::MD5MismatchedFrameInfo
    : public VideoFrameValidator::MismatchedFrameInfo {
  MD5MismatchedFrameInfo(size_t frame_index,
                         const std::string& computed_md5,
                         const std::string& expected_md5)
      : MismatchedFrameInfo(frame_index),
        computed_md5(computed_md5),
        expected_md5(expected_md5) {}
  ~MD5MismatchedFrameInfo() override = default;
  void Print() const override {
    LOG(ERROR) << "frame_index: " << frame_index
               << ", computed_md5: " << computed_md5
               << ", expected_md5: " << expected_md5;
  }

  const std::string computed_md5;
  const std::string expected_md5;
};

// static
std::unique_ptr<MD5VideoFrameValidator> MD5VideoFrameValidator::Create(
    const std::vector<std::string>& expected_frame_checksums,
    VideoPixelFormat validation_format,
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor,
    CropHelper crop_helper) {
  auto video_frame_validator = base::WrapUnique(new MD5VideoFrameValidator(
      expected_frame_checksums, validation_format,
      std::move(corrupt_frame_processor), std::move(crop_helper)));
  if (!video_frame_validator->Initialize()) {
    LOG(ERROR) << "Failed to initialize MD5VideoFrameValidator.";
    return nullptr;
  }

  return video_frame_validator;
}

MD5VideoFrameValidator::MD5VideoFrameValidator(
    const std::vector<std::string>& expected_frame_checksums,
    VideoPixelFormat validation_format,
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor,
    CropHelper crop_helper)
    : VideoFrameValidator(std::move(corrupt_frame_processor),
                          std::move(crop_helper)),
      expected_frame_checksums_(expected_frame_checksums),
      validation_format_(validation_format) {}

MD5VideoFrameValidator::~MD5VideoFrameValidator() = default;

std::unique_ptr<VideoFrameValidator::MismatchedFrameInfo>
MD5VideoFrameValidator::Validate(scoped_refptr<const VideoFrame> frame,
                                 size_t frame_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);
#if BUILDFLAG(IS_CHROMEOS)
  // b/149808895: There is a bug in the synchronization on mapped buffers, which
  // causes the frame validation failure. The bug is due to some missing i915
  // patches in kernel v3.18. The bug will be fixed if the kernel is upreved to
  // v4.4 or newer. Inserts usleep as a short term workaround to the
  // synchronization bug until the kernel uprev is complete for all the v3.18
  // devices. Since this bug only occurs in Skylake just because they are 3.18
  // devices, we also filter by the processor.
  const static std::string kernel_version = base::SysInfo::KernelVersion();
  if (base::StartsWith(kernel_version, "3.18")) {
    static const bool is_skylake = []() {
      constexpr int kPentiumAndLaterFamily = 0x06;
      constexpr int kSkyLakeModelId = 0x5E;
      constexpr int kSkyLake_LModelId = 0x4E;

      const base::CPU& cpuid = base::CPU::GetInstanceNoAllocation();
      return cpuid.family() == kPentiumAndLaterFamily &&
             (cpuid.model() == kSkyLakeModelId ||
              cpuid.model() == kSkyLake_LModelId);
    }();

    if (is_skylake)
      usleep(10);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  if (frame->format() != validation_format_) {
    frame = ConvertVideoFrame(frame.get(), validation_format_);
  }
  CHECK(frame);

  std::string computed_md5 = ComputeMD5FromVideoFrame(*frame);
  if (expected_frame_checksums_.size() > 0) {
    LOG_IF(FATAL, frame_index >= expected_frame_checksums_.size())
        << "Frame number is over than the number of read md5 values in file.";
    const auto& expected_md5 = expected_frame_checksums_[frame_index];
    if (computed_md5 != expected_md5)
      return std::make_unique<MD5MismatchedFrameInfo>(frame_index, computed_md5,
                                                      expected_md5);
  }
  return nullptr;
}

std::string MD5VideoFrameValidator::ComputeMD5FromVideoFrame(
    const VideoFrame& video_frame) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);
  base::MD5Context context;
  base::MD5Init(&context);

  // VideoFrame::HashFrameForTesting() computes MD5 hash values of the coded
  // area. However, MD5 hash values used in our test only use the visible area
  // because they are computed from images output by decode tools like ffmpeg.
  const VideoPixelFormat format = video_frame.format();
  const gfx::Rect& visible_rect = video_frame.visible_rect();
  for (size_t i = 0; i < VideoFrame::NumPlanes(format); ++i) {
    const size_t visible_row_bytes =
        VideoFrame::RowBytes(i, format, visible_rect.width());
    const int visible_rows = VideoFrame::Rows(i, format, visible_rect.height());
    const size_t stride = video_frame.stride(i);
    for (int row = 0; row < visible_rows; ++row) {
      base::MD5Update(&context, base::span<const uint8_t>(
                                    video_frame.data(i) + (stride * row),
                                    visible_row_bytes));
    }
  }
  base::MD5Digest digest;
  base::MD5Final(&digest, &context);
  return MD5DigestToBase16(digest);
}

struct RawVideoFrameValidator::RawMismatchedFrameInfo
    : public VideoFrameValidator::MismatchedFrameInfo {
  RawMismatchedFrameInfo(size_t frame_index, size_t diff_cnt)
      : MismatchedFrameInfo(frame_index), diff_cnt(diff_cnt) {}
  ~RawMismatchedFrameInfo() override = default;
  void Print() const override {
    LOG(ERROR) << "frame_index: " << frame_index << ", diff_cnt: " << diff_cnt;
  }

  size_t diff_cnt;
};

// static
std::unique_ptr<RawVideoFrameValidator> RawVideoFrameValidator::Create(
    const GetModelFrameCB& get_model_frame_cb,
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor,
    uint8_t tolerance,
    CropHelper crop_helper) {
  auto video_frame_validator = base::WrapUnique(new RawVideoFrameValidator(
      get_model_frame_cb, std::move(corrupt_frame_processor), tolerance,
      std::move(crop_helper)));
  if (!video_frame_validator->Initialize()) {
    LOG(ERROR) << "Failed to initialize RawVideoFrameValidator.";
    return nullptr;
  }

  return video_frame_validator;
}

RawVideoFrameValidator::RawVideoFrameValidator(
    const GetModelFrameCB& get_model_frame_cb,
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor,
    uint8_t tolerance,
    CropHelper crop_helper)
    : VideoFrameValidator(std::move(corrupt_frame_processor),
                          std::move(crop_helper)),
      get_model_frame_cb_(get_model_frame_cb),
      tolerance_(tolerance) {}

RawVideoFrameValidator::~RawVideoFrameValidator() = default;

std::unique_ptr<VideoFrameValidator::MismatchedFrameInfo>
RawVideoFrameValidator::Validate(scoped_refptr<const VideoFrame> frame,
                                 size_t frame_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);
  auto model_frame = get_model_frame_cb_.Run(frame_index);
  CHECK(model_frame);

  if (ShouldCrop())
    model_frame = CloneAndCropFrame(std::move(model_frame));

  size_t diff_cnt =
      CompareFramesWithErrorDiff(*frame, *model_frame, tolerance_);
  if (diff_cnt > 0)
    return std::make_unique<RawMismatchedFrameInfo>(frame_index, diff_cnt);
  return nullptr;
}

struct PSNRVideoFrameValidator::PSNRMismatchedFrameInfo
    : public VideoFrameValidator::MismatchedFrameInfo {
  PSNRMismatchedFrameInfo(size_t frame_index, double psnr)
      : MismatchedFrameInfo(frame_index), psnr(psnr) {}
  ~PSNRMismatchedFrameInfo() override = default;
  void Print() const override {
    LOG(ERROR) << "frame_index: " << frame_index << ", psnr: " << psnr;
  }

  double psnr;
};

// static
std::unique_ptr<PSNRVideoFrameValidator> PSNRVideoFrameValidator::Create(
    const GetModelFrameCB& get_model_frame_cb,
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor,
    ValidationMode validation_mode,
    double tolerance,
    CropHelper crop_helper) {
  auto video_frame_validator = base::WrapUnique(new PSNRVideoFrameValidator(
      get_model_frame_cb, std::move(corrupt_frame_processor), validation_mode,
      tolerance, std::move(crop_helper)));
  if (!video_frame_validator->Initialize()) {
    LOG(ERROR) << "Failed to initialize PSNRVideoFrameValidator.";
    return nullptr;
  }
  return video_frame_validator;
}

PSNRVideoFrameValidator::PSNRVideoFrameValidator(
    const GetModelFrameCB& get_model_frame_cb,
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor,
    ValidationMode validation_mode,
    double tolerance,
    CropHelper crop_helper)
    : VideoFrameValidator(std::move(corrupt_frame_processor),
                          std::move(crop_helper)),
      get_model_frame_cb_(get_model_frame_cb),
      tolerance_(tolerance),
      validation_mode_(validation_mode) {}

PSNRVideoFrameValidator::~PSNRVideoFrameValidator() = default;

std::unique_ptr<VideoFrameValidator::MismatchedFrameInfo>
PSNRVideoFrameValidator::Validate(scoped_refptr<const VideoFrame> frame,
                                  size_t frame_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);
  auto model_frame = get_model_frame_cb_.Run(frame_index);
  CHECK(model_frame);

  if (ShouldCrop())
    model_frame = CloneAndCropFrame(std::move(model_frame));

  double psnr = ComputePSNR(*frame, *model_frame);
  DVLOGF(4) << "frame_index: " << frame_index << ", psnr: " << psnr;
  psnr_[frame_index] = psnr;
  if (psnr < tolerance_)
    return std::make_unique<PSNRMismatchedFrameInfo>(frame_index, psnr);
  return nullptr;
}

bool PSNRVideoFrameValidator::Passed() const {
  if (validation_mode_ == ValidationMode::kThreshold)
    return GetMismatchedFramesCount() == 0u;
  if (psnr_.empty())
    return true;

  double average = 0;
  for (const auto& psnr : psnr_) {
    average += psnr.second;
  }
  average /= psnr_.size();
  DVLOGF(4) << "Average PSNR: " << average;
  if (average < tolerance_) {
    LOG(ERROR) << "Average PSNR is too low: " << average;
    return false;
  }
  return true;
}

struct SSIMVideoFrameValidator::SSIMMismatchedFrameInfo
    : public VideoFrameValidator::MismatchedFrameInfo {
  SSIMMismatchedFrameInfo(size_t frame_index, double ssim)
      : MismatchedFrameInfo(frame_index), ssim(ssim) {}
  ~SSIMMismatchedFrameInfo() override = default;
  void Print() const override {
    LOG(ERROR) << "frame_index: " << frame_index << ", ssim: " << ssim;
  }

  double ssim;
};

// static
std::unique_ptr<SSIMVideoFrameValidator> SSIMVideoFrameValidator::Create(
    const GetModelFrameCB& get_model_frame_cb,
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor,
    ValidationMode validation_mode,
    double tolerance,
    CropHelper crop_helper) {
  auto video_frame_validator = base::WrapUnique(new SSIMVideoFrameValidator(
      get_model_frame_cb, std::move(corrupt_frame_processor), validation_mode,
      tolerance, std::move(crop_helper)));
  if (!video_frame_validator->Initialize()) {
    LOG(ERROR) << "Failed to initialize SSIMVideoFrameValidator.";
    return nullptr;
  }
  return video_frame_validator;
}

SSIMVideoFrameValidator::SSIMVideoFrameValidator(
    const GetModelFrameCB& get_model_frame_cb,
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor,
    ValidationMode validation_mode,
    double tolerance,
    CropHelper crop_helper)
    : VideoFrameValidator(std::move(corrupt_frame_processor),
                          std::move(crop_helper)),
      get_model_frame_cb_(get_model_frame_cb),
      tolerance_(tolerance),
      validation_mode_(validation_mode) {}

SSIMVideoFrameValidator::~SSIMVideoFrameValidator() = default;

std::unique_ptr<VideoFrameValidator::MismatchedFrameInfo>
SSIMVideoFrameValidator::Validate(scoped_refptr<const VideoFrame> frame,
                                  size_t frame_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);
  auto model_frame = get_model_frame_cb_.Run(frame_index);
  CHECK(model_frame);

  if (ShouldCrop())
    model_frame = CloneAndCropFrame(std::move(model_frame));

  double ssim = ComputeSSIM(*frame, *model_frame);
  DVLOGF(4) << "frame_index: " << frame_index << ", ssim: " << ssim;
  ssim_[frame_index] = ssim;
  if (ssim < tolerance_)
    return std::make_unique<SSIMMismatchedFrameInfo>(frame_index, ssim);
  return nullptr;
}

bool SSIMVideoFrameValidator::Passed() const {
  if (validation_mode_ == ValidationMode::kThreshold)
    return GetMismatchedFramesCount() == 0u;
  if (ssim_.empty())
    return true;

  double average = 0;
  for (const auto& ssim : ssim_) {
    average += ssim.second;
  }
  average /= ssim_.size();
  DVLOGF(4) << "Average SSIM: " << average;
  if (average < tolerance_) {
    LOG(ERROR) << "Average SSIM is too low: " << average;
    return false;
  }
  return true;
}

struct LogLikelihoodRatioVideoFrameValidator::
    LogLikelihoodRatioMismatchedFrameInfo
    : public VideoFrameValidator::MismatchedFrameInfo {
  LogLikelihoodRatioMismatchedFrameInfo(size_t frame_index, double ratio)
      : MismatchedFrameInfo(frame_index), ratio(ratio) {}
  ~LogLikelihoodRatioMismatchedFrameInfo() override = default;
  void Print() const override {
    LOG(ERROR) << "frame_index: " << frame_index
               << ", log likelihood ratio: " << ratio;
  }

  double ratio;
};

// static
std::unique_ptr<LogLikelihoodRatioVideoFrameValidator>
LogLikelihoodRatioVideoFrameValidator::Create(
    const GetModelFrameCB& get_model_frame_cb,
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor,
    ValidationMode validation_mode,
    double tolerance,
    CropHelper crop_helper) {
  auto video_frame_validator =
      base::WrapUnique(new LogLikelihoodRatioVideoFrameValidator(
          get_model_frame_cb, std::move(corrupt_frame_processor),
          validation_mode, tolerance, std::move(crop_helper)));
  if (!video_frame_validator->Initialize()) {
    LOG(ERROR) << "Failed to initialize LogLikelihoodRatioVideoFrameValidator.";
    return nullptr;
  }

  return video_frame_validator;
}

LogLikelihoodRatioVideoFrameValidator::LogLikelihoodRatioVideoFrameValidator(
    const GetModelFrameCB& get_model_frame_cb,
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor,
    ValidationMode validation_mode,
    double tolerance,
    CropHelper crop_helper)
    : VideoFrameValidator(std::move(corrupt_frame_processor),
                          std::move(crop_helper)),
      get_model_frame_cb_(get_model_frame_cb),
      tolerance_(tolerance) {}

LogLikelihoodRatioVideoFrameValidator::
    ~LogLikelihoodRatioVideoFrameValidator() = default;

std::unique_ptr<VideoFrameValidator::MismatchedFrameInfo>
LogLikelihoodRatioVideoFrameValidator::Validate(
    scoped_refptr<const VideoFrame> frame,
    size_t frame_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);
  auto model_frame = get_model_frame_cb_.Run(frame_index);

  CHECK(model_frame);

  if (ShouldCrop()) {
    model_frame = CloneAndCropFrame(std::move(model_frame));
  }

  double ratio = ComputeLogLikelihoodRatio(model_frame, frame);
  DVLOGF(4) << "frame_index: " << frame_index
            << ", log likelihood ratio: " << ratio;
  log_likelihood_ratios_[frame_index] = ratio;
  if (ratio >= tolerance_) {
    return std::make_unique<LogLikelihoodRatioMismatchedFrameInfo>(frame_index,
                                                                   ratio);
  }

  return nullptr;
}

}  // namespace test
}  // namespace media
