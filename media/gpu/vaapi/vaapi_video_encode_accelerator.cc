// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_video_encode_accelerator.h"

#include <string.h>

#include <algorithm>
#include <memory>
#include <utility>

#include <va/va.h>
#include <va/va_enc_h264.h>
#include <va/va_enc_vp8.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/unaligned_shared_memory.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/gpu/h264_dpb.h"
#include "media/gpu/vaapi/h264_encoder.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vp8_encoder.h"
#include "media/gpu/vp8_reference_frame_vector.h"

#define VLOGF(level) VLOG(level) << __func__ << "(): "
#define DVLOGF(level) DVLOG(level) << __func__ << "(): "

#define NOTIFY_ERROR(error, msg)                        \
  do {                                                  \
    SetState(kError);                                   \
    VLOGF(1) << msg;                                    \
    VLOGF(1) << "Calling NotifyError(" << error << ")"; \
    NotifyError(error);                                 \
  } while (0)

namespace media {

namespace {
// Minimum number of frames in flight for pipeline depth, adjust to this number
// if encoder requests less.
constexpr size_t kMinNumFramesInFlight = 4;

// Need 2 surfaces for each frame: one for input data and one for
// reconstructed picture, which is later used for reference.
constexpr size_t kNumSurfacesPerFrame = 2;

// Percentage of bitrate set to be targeted by the HW encoder.
constexpr unsigned int kTargetBitratePercentage = 90;

// UMA errors that the VaapiVideoEncodeAccelerator class reports.
enum VAVEAEncoderFailure {
  VAAPI_ERROR = 0,
  VAVEA_ENCODER_FAILURES_MAX,
};

static void ReportToUMA(VAVEAEncoderFailure failure) {
  UMA_HISTOGRAM_ENUMERATION("Media.VAVEA.EncoderFailure", failure,
                            VAVEA_ENCODER_FAILURES_MAX + 1);
}

}  // namespace

// Encode job for one frame. Created when an input frame is awaiting and
// enough resources are available to proceed. Once the job is prepared and
// submitted to the hardware, it awaits on the |submitted_encode_jobs_| queue
// for an output bitstream buffer to become available. Once one is ready,
// the encoded bytes are downloaded to it, job resources are released
// and become available for reuse.
class VaapiEncodeJob : public AcceleratedVideoEncoder::EncodeJob {
 public:
  VaapiEncodeJob(scoped_refptr<VideoFrame> input_frame,
                 bool keyframe,
                 base::OnceClosure execute_cb,
                 scoped_refptr<VASurface> input_surface,
                 scoped_refptr<VASurface> reconstructed_surface,
                 VABufferID coded_buffer_id);

  VaapiEncodeJob* AsVaapiEncodeJob() override { return this; }

  VABufferID coded_buffer_id() const { return coded_buffer_id_; }
  const scoped_refptr<VASurface> input_surface() const {
    return input_surface_;
  }
  const scoped_refptr<VASurface> reconstructed_surface() const {
    return reconstructed_surface_;
  }

 private:
  ~VaapiEncodeJob() override = default;

  // Input surface for video frame data.
  const scoped_refptr<VASurface> input_surface_;

  // Surface for the reconstructed picture, used for reference
  // for subsequent frames.
  const scoped_refptr<VASurface> reconstructed_surface_;

  // Buffer that will contain the output bitstream data for this frame.
  VABufferID coded_buffer_id_;

  DISALLOW_COPY_AND_ASSIGN(VaapiEncodeJob);
};

struct VaapiVideoEncodeAccelerator::InputFrameRef {
  InputFrameRef(scoped_refptr<VideoFrame> frame, bool force_keyframe)
      : frame(frame), force_keyframe(force_keyframe) {}
  const scoped_refptr<VideoFrame> frame;
  const bool force_keyframe;
};

struct VaapiVideoEncodeAccelerator::BitstreamBufferRef {
  BitstreamBufferRef(int32_t id, const BitstreamBuffer& buffer)
      : id(id),
        shm(std::make_unique<UnalignedSharedMemory>(buffer.handle(),
                                                    buffer.size(),
                                                    false)),
        offset(buffer.offset()) {}
  const int32_t id;
  const std::unique_ptr<UnalignedSharedMemory> shm;
  const off_t offset;
};

VideoEncodeAccelerator::SupportedProfiles
VaapiVideoEncodeAccelerator::GetSupportedProfiles() {
  return VaapiWrapper::GetSupportedEncodeProfiles();
}

class VaapiVideoEncodeAccelerator::H264Accelerator
    : public H264Encoder::Accelerator {
 public:
  explicit H264Accelerator(VaapiVideoEncodeAccelerator* vea) : vea_(vea) {}

  ~H264Accelerator() override = default;

  // H264Encoder::Accelerator implementation.
  scoped_refptr<H264Picture> GetPicture(
      AcceleratedVideoEncoder::EncodeJob* job) override;

  bool SubmitPackedHeaders(
      AcceleratedVideoEncoder::EncodeJob* job,
      scoped_refptr<H264BitstreamBuffer> packed_sps,
      scoped_refptr<H264BitstreamBuffer> packed_pps) override;

  bool SubmitFrameParameters(
      AcceleratedVideoEncoder::EncodeJob* job,
      const H264Encoder::EncodeParams& encode_params,
      const H264SPS& sps,
      const H264PPS& pps,
      scoped_refptr<H264Picture> pic,
      const std::list<scoped_refptr<H264Picture>>& ref_pic_list0,
      const std::list<scoped_refptr<H264Picture>>& ref_pic_list1) override;

 private:
  VaapiVideoEncodeAccelerator* const vea_;
};

class VaapiVideoEncodeAccelerator::VP8Accelerator
    : public VP8Encoder::Accelerator {
 public:
  explicit VP8Accelerator(VaapiVideoEncodeAccelerator* vea) : vea_(vea) {}

  ~VP8Accelerator() override = default;

  // VP8Encoder::Accelerator implementation.
  scoped_refptr<VP8Picture> GetPicture(
      AcceleratedVideoEncoder::EncodeJob* job) override;

  bool SubmitFrameParameters(
      AcceleratedVideoEncoder::EncodeJob* job,
      const VP8Encoder::EncodeParams& encode_params,
      scoped_refptr<VP8Picture> pic,
      const Vp8ReferenceFrameVector& ref_frames) override;

 private:
  VaapiVideoEncodeAccelerator* const vea_;
};

VaapiVideoEncodeAccelerator::VaapiVideoEncodeAccelerator()
    : codec_(kUnknownVideoCodec),
      output_buffer_byte_size_(0),
      state_(kUninitialized),
      encoder_thread_("VAVEAEncoderThread"),
      child_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_this_ptr_factory_(this) {
  VLOGF(2);
}

VaapiVideoEncodeAccelerator::~VaapiVideoEncodeAccelerator() {
  VLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK(!encoder_thread_.IsRunning());
}

bool VaapiVideoEncodeAccelerator::Initialize(const Config& config,
                                             Client* client) {
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK(!encoder_thread_.IsRunning());
  DCHECK_EQ(state_, kUninitialized);

  VLOGF(2) << "Initializing VAVEA, " << config.AsHumanReadableString();

  client_ptr_factory_.reset(new base::WeakPtrFactory<Client>(client));
  client_ = client_ptr_factory_->GetWeakPtr();

  codec_ = VideoCodecProfileToVideoCodec(config.output_profile);
  if (codec_ != kCodecH264 && codec_ != kCodecVP8) {
    VLOGF(1) << "Unsupported profile: "
             << GetProfileName(config.output_profile);
    return false;
  }

  if (codec_ == kCodecVP8 &&
      config.content_type == Config::ContentType::kDisplay) {
    VLOGF(1) << "Vaapi VP8 encoder does not currently support screen content.";
    return false;
  }

  switch (config.input_format) {
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_NV12:
      break;
    default:
      VLOGF(1) << "Unsupported input format: " << config.input_format;
      return false;
  }

  const SupportedProfiles& profiles = GetSupportedProfiles();
  auto profile = find_if(profiles.begin(), profiles.end(),
                         [output_profile = config.output_profile](
                             const SupportedProfile& profile) {
                           return profile.profile == output_profile;
                         });
  if (profile == profiles.end()) {
    VLOGF(1) << "Unsupported output profile "
             << GetProfileName(config.output_profile);
    return false;
  }

  if (config.input_visible_size.width() > profile->max_resolution.width() ||
      config.input_visible_size.height() > profile->max_resolution.height()) {
    VLOGF(1) << "Input size too big: " << config.input_visible_size.ToString()
             << ", max supported size: " << profile->max_resolution.ToString();
    return false;
  }

  vaapi_wrapper_ = VaapiWrapper::CreateForVideoCodec(
      VaapiWrapper::kEncode, config.output_profile,
      base::Bind(&ReportToUMA, VAAPI_ERROR));
  if (!vaapi_wrapper_) {
    VLOGF(1) << "Failed initializing VAAPI for profile "
             << GetProfileName(config.output_profile);
    return false;
  }

  if (!encoder_thread_.Start()) {
    VLOGF(1) << "Failed to start encoder thread";
    return false;
  }

  encoder_thread_task_runner_ = encoder_thread_.task_runner();

  // Finish remaining initialization on the encoder thread.
  encoder_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VaapiVideoEncodeAccelerator::InitializeTask,
                                base::Unretained(this), config));
  return true;
}

void VaapiVideoEncodeAccelerator::InitializeTask(const Config& config) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kUninitialized);
  VLOGF(2);

  weak_this_ = weak_this_ptr_factory_.GetWeakPtr();

  switch (codec_) {
    case kCodecH264:
      encoder_ = std::make_unique<H264Encoder>(
          std::make_unique<H264Accelerator>(this));
      break;

    case kCodecVP8:
      encoder_ =
          std::make_unique<VP8Encoder>(std::make_unique<VP8Accelerator>(this));
      break;

    default:
      NOTREACHED() << "Unsupported codec type " << GetCodecName(codec_);
      return;
  }

  if (!encoder_->Initialize(config)) {
    NOTIFY_ERROR(kInvalidArgumentError, "Failed initializing encoder");
    return;
  }

  coded_size_ = encoder_->GetCodedSize();
  output_buffer_byte_size_ = encoder_->GetBitstreamBufferSize();
  const size_t max_ref_frames = encoder_->GetMaxNumOfRefFrames();
  // Use at least kMinNumFramesInFlight if encoder requested less for
  // pipeline depth.
  const size_t num_frames_in_flight =
      std::max(kMinNumFramesInFlight, max_ref_frames);
  const size_t num_surfaces = (num_frames_in_flight + 1) * kNumSurfacesPerFrame;
  DVLOGF(1) << "Frames in flight: " << num_frames_in_flight;

  va_surface_release_cb_ = BindToCurrentLoop(
      base::Bind(&VaapiVideoEncodeAccelerator::RecycleVASurfaceID,
                 base::Unretained(this)));

  if (!vaapi_wrapper_->CreateSurfaces(VA_RT_FORMAT_YUV420, coded_size_,
                                      num_surfaces,
                                      &available_va_surface_ids_)) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed creating VASurfaces");
    return;
  }

  child_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::RequireBitstreamBuffers, client_,
                                num_frames_in_flight, coded_size_,
                                output_buffer_byte_size_));

  SetState(kEncoding);
}

void VaapiVideoEncodeAccelerator::RecycleVASurfaceID(
    VASurfaceID va_surface_id) {
  DVLOGF(4) << "va_surface_id: " << va_surface_id;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  available_va_surface_ids_.push_back(va_surface_id);
  EncodePendingInputs();
}

void VaapiVideoEncodeAccelerator::ExecuteEncode(VASurfaceID va_surface_id) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  if (!vaapi_wrapper_->ExecuteAndDestroyPendingBuffers(va_surface_id))
    NOTIFY_ERROR(kPlatformFailureError, "Failed to execute encode");
}

void VaapiVideoEncodeAccelerator::UploadFrame(scoped_refptr<VideoFrame> frame,
                                              VASurfaceID va_surface_id) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  DVLOGF(4) << "frame is uploading: " << va_surface_id;
  if (!vaapi_wrapper_->UploadVideoFrameToSurface(frame, va_surface_id))
    NOTIFY_ERROR(kPlatformFailureError, "Failed to upload frame");
}

void VaapiVideoEncodeAccelerator::SubmitBuffer(
    VABufferType type,
    scoped_refptr<base::RefCountedBytes> buffer) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  if (!vaapi_wrapper_->SubmitBuffer(type, buffer->size(), buffer->front()))
    NOTIFY_ERROR(kPlatformFailureError, "Failed submitting a buffer");
}

void VaapiVideoEncodeAccelerator::SubmitVAEncMiscParamBuffer(
    VAEncMiscParameterType type,
    scoped_refptr<base::RefCountedBytes> buffer) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  if (!vaapi_wrapper_->SubmitVAEncMiscParamBuffer(type, buffer->size(),
                                                  buffer->front())) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed submitting a parameter buffer");
  }
}

void VaapiVideoEncodeAccelerator::SubmitH264BitstreamBuffer(
    scoped_refptr<H264BitstreamBuffer> buffer) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  if (!vaapi_wrapper_->SubmitBuffer(VAEncPackedHeaderDataBufferType,
                                    buffer->BytesInBuffer(), buffer->data())) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed submitting a bitstream buffer");
  }
}

void VaapiVideoEncodeAccelerator::TryToReturnBitstreamBuffer() {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  if (state_ != kEncoding)
    return;

  while (!submitted_encode_jobs_.empty() &&
         submitted_encode_jobs_.front() == nullptr) {
    // A null job indicates a flush command.
    submitted_encode_jobs_.pop();
    DVLOGF(2) << "FlushDone";
    DCHECK(flush_callback_);
    child_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(flush_callback_), true));
  }

  if (submitted_encode_jobs_.empty() || available_bitstream_buffers_.empty())
    return;

  auto buffer = std::move(available_bitstream_buffers_.front());
  available_bitstream_buffers_.pop();
  auto encode_job = submitted_encode_jobs_.front();
  submitted_encode_jobs_.pop();

  ReturnBitstreamBuffer(encode_job, std::move(buffer));
}

void VaapiVideoEncodeAccelerator::ReturnBitstreamBuffer(
    scoped_refptr<VaapiEncodeJob> encode_job,
    std::unique_ptr<BitstreamBufferRef> buffer) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  uint8_t* target_data = static_cast<uint8_t*>(buffer->shm->memory());
  size_t data_size = 0;

  if (!vaapi_wrapper_->DownloadAndDestroyCodedBuffer(
          encode_job->coded_buffer_id(), encode_job->input_surface()->id(),
          target_data, buffer->shm->size(), &data_size)) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed downloading coded buffer");
    return;
  }

  DVLOGF(4) << "Returning bitstream buffer "
            << (encode_job->IsKeyframeRequested() ? "(keyframe)" : "")
            << " id: " << buffer->id << " size: " << data_size;

  child_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::BitstreamBufferReady, client_,
                                buffer->id, encode_job->Metadata(data_size)));
}

void VaapiVideoEncodeAccelerator::Encode(const scoped_refptr<VideoFrame>& frame,
                                         bool force_keyframe) {
  DVLOGF(4) << "Frame timestamp: " << frame->timestamp().InMilliseconds()
            << " force_keyframe: " << force_keyframe;
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  encoder_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VaapiVideoEncodeAccelerator::EncodeTask,
                                base::Unretained(this), frame, force_keyframe));
}

void VaapiVideoEncodeAccelerator::EncodeTask(scoped_refptr<VideoFrame> frame,
                                             bool force_keyframe) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(state_, kUninitialized);

  input_queue_.push(std::make_unique<InputFrameRef>(frame, force_keyframe));
  EncodePendingInputs();
}

scoped_refptr<VaapiEncodeJob> VaapiVideoEncodeAccelerator::CreateEncodeJob(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  if (available_va_surface_ids_.size() < kNumSurfacesPerFrame) {
    DVLOGF(4) << "Not enough surfaces available";
    return nullptr;
  }

  VABufferID coded_buffer_id;
  if (!vaapi_wrapper_->CreateCodedBuffer(output_buffer_byte_size_,
                                         &coded_buffer_id)) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed creating coded buffer");
    return nullptr;
  }

  static_assert(kNumSurfacesPerFrame == 2, "kNumSurfacesPerFrame must be 2");
  scoped_refptr<VASurface> input_surface = new VASurface(
      available_va_surface_ids_.back(), coded_size_,
      vaapi_wrapper_->va_surface_format(), va_surface_release_cb_);
  available_va_surface_ids_.pop_back();

  scoped_refptr<VASurface> reconstructed_surface = new VASurface(
      available_va_surface_ids_.back(), coded_size_,
      vaapi_wrapper_->va_surface_format(), va_surface_release_cb_);
  available_va_surface_ids_.pop_back();

  auto job = base::MakeRefCounted<VaapiEncodeJob>(
      frame, force_keyframe,
      base::BindOnce(&VaapiVideoEncodeAccelerator::ExecuteEncode,
                     base::Unretained(this), input_surface->id()),
      input_surface, reconstructed_surface, coded_buffer_id);

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::UploadFrame,
                     base::Unretained(this), frame, input_surface->id()));

  return job;
}

void VaapiVideoEncodeAccelerator::EncodePendingInputs() {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  DVLOGF(4);

  while (state_ == kEncoding && !input_queue_.empty()) {
    const std::unique_ptr<InputFrameRef>& input_frame = input_queue_.front();

    // If this is a flush (null) frame, don't create/submit a new encode job for
    // it, but forward a null job to the submitted_encode_jobs_ queue.
    scoped_refptr<VaapiEncodeJob> job;
    if (input_frame) {
      job = CreateEncodeJob(input_frame->frame, input_frame->force_keyframe);
      if (!job)
        return;
    }

    input_queue_.pop();

    if (job) {
      if (!encoder_->PrepareEncodeJob(job.get())) {
        NOTIFY_ERROR(kPlatformFailureError, "Failed preparing an encode job.");
        return;
      }

      TRACE_EVENT0("media,gpu", "VAVEA: Execute");
      job->Execute();
    }

    submitted_encode_jobs_.push(job);
    TryToReturnBitstreamBuffer();
  }
}

void VaapiVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    const BitstreamBuffer& buffer) {
  DVLOGF(4) << "id: " << buffer.id();
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  if (buffer.size() < output_buffer_byte_size_) {
    NOTIFY_ERROR(kInvalidArgumentError, "Provided bitstream buffer too small");
    return;
  }

  auto buffer_ref = std::make_unique<BitstreamBufferRef>(buffer.id(), buffer);

  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VaapiVideoEncodeAccelerator::UseOutputBitstreamBufferTask,
                 base::Unretained(this), base::Passed(&buffer_ref)));
}

void VaapiVideoEncodeAccelerator::UseOutputBitstreamBufferTask(
    std::unique_ptr<BitstreamBufferRef> buffer_ref) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(state_, kUninitialized);

  if (!buffer_ref->shm->MapAt(buffer_ref->offset, buffer_ref->shm->size())) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed mapping shared memory.");
    return;
  }

  available_bitstream_buffers_.push(std::move(buffer_ref));
  TryToReturnBitstreamBuffer();
}

void VaapiVideoEncodeAccelerator::RequestEncodingParametersChange(
    uint32_t bitrate,
    uint32_t framerate) {
  VLOGF(2) << "bitrate: " << bitrate << " framerate: " << framerate;
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  VideoBitrateAllocation allocation;
  allocation.SetBitrate(0, 0, bitrate);
  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &VaapiVideoEncodeAccelerator::RequestEncodingParametersChangeTask,
          base::Unretained(this), allocation, framerate));
}

void VaapiVideoEncodeAccelerator::RequestEncodingParametersChange(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate) {
  VLOGF(2) << "bitrate: " << bitrate_allocation.GetSumBps()
           << " framerate: " << framerate;
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &VaapiVideoEncodeAccelerator::RequestEncodingParametersChangeTask,
          base::Unretained(this), bitrate_allocation, framerate));
}

void VaapiVideoEncodeAccelerator::RequestEncodingParametersChangeTask(
    VideoBitrateAllocation bitrate_allocation,
    uint32_t framerate) {
  VLOGF(2) << "bitrate: " << bitrate_allocation.GetSumBps()
           << " framerate: " << framerate;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(state_, kUninitialized);

  if (!encoder_->UpdateRates(bitrate_allocation, framerate)) {
    VLOGF(1) << "Failed to update rates to " << bitrate_allocation.GetSumBps()
             << " " << framerate;
  }
}

void VaapiVideoEncodeAccelerator::Flush(FlushCallback flush_callback) {
  DVLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  if (flush_callback_) {
    NOTIFY_ERROR(kIllegalStateError, "There is a pending flush");
    std::move(flush_callback).Run(false);
    return;
  }
  flush_callback_ = std::move(flush_callback);
  encoder_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VaapiVideoEncodeAccelerator::FlushTask,
                                base::Unretained(this)));
}

void VaapiVideoEncodeAccelerator::FlushTask() {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  // Insert an null job to indicate a flush command.
  input_queue_.push(std::unique_ptr<InputFrameRef>(nullptr));
  EncodePendingInputs();
}

bool VaapiVideoEncodeAccelerator::IsFlushSupported() {
  return true;
}

void VaapiVideoEncodeAccelerator::Destroy() {
  DVLOGF(2);
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  if (encoder_thread_.IsRunning()) {
    encoder_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&VaapiVideoEncodeAccelerator::DestroyTask,
                                  base::Unretained(this)));
    encoder_thread_.Stop();
  }

  if (flush_callback_)
    std::move(flush_callback_).Run(false);

  weak_this_ptr_factory_.InvalidateWeakPtrs();
  delete this;
}

void VaapiVideoEncodeAccelerator::DestroyTask() {
  VLOGF(2);
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  // Clean up members that are to be accessed on the encoder thread only.
  available_va_surface_ids_.clear();
  available_va_buffer_ids_.clear();

  while (!available_bitstream_buffers_.empty())
    available_bitstream_buffers_.pop();

  while (!input_queue_.empty())
    input_queue_.pop();

  while (!submitted_encode_jobs_.empty())
    submitted_encode_jobs_.pop();

  encoder_ = nullptr;
}

void VaapiVideoEncodeAccelerator::SetState(State state) {
  // Only touch state on encoder thread, unless it's not running.
  if (encoder_thread_.IsRunning() &&
      !encoder_thread_task_runner_->BelongsToCurrentThread()) {
    encoder_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VaapiVideoEncodeAccelerator::SetState,
                                  base::Unretained(this), state));
    return;
  }

  VLOGF(2) << "setting state to: " << state;
  state_ = state;
}

void VaapiVideoEncodeAccelerator::NotifyError(Error error) {
  if (!child_task_runner_->BelongsToCurrentThread()) {
    child_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VaapiVideoEncodeAccelerator::NotifyError,
                                  weak_this_, error));
    return;
  }

  if (client_) {
    client_->NotifyError(error);
    client_ptr_factory_.reset();
  }
}

VaapiEncodeJob::VaapiEncodeJob(scoped_refptr<VideoFrame> input_frame,
                               bool keyframe,
                               base::OnceClosure execute_cb,
                               scoped_refptr<VASurface> input_surface,
                               scoped_refptr<VASurface> reconstructed_surface,
                               VABufferID coded_buffer_id)
    : EncodeJob(input_frame, keyframe, std::move(execute_cb)),
      input_surface_(input_surface),
      reconstructed_surface_(reconstructed_surface),
      coded_buffer_id_(coded_buffer_id) {
  DCHECK(input_surface_);
  DCHECK(reconstructed_surface_);
  DCHECK_NE(coded_buffer_id_, VA_INVALID_ID);
}

static void InitVAPictureH264(VAPictureH264* va_pic) {
  *va_pic = {};
  va_pic->picture_id = VA_INVALID_ID;
  va_pic->flags = VA_PICTURE_H264_INVALID;
}

static scoped_refptr<base::RefCountedBytes> MakeRefCountedBytes(void* ptr,
                                                                size_t size) {
  return base::MakeRefCounted<base::RefCountedBytes>(
      reinterpret_cast<uint8_t*>(ptr), size);
}

bool VaapiVideoEncodeAccelerator::H264Accelerator::SubmitFrameParameters(
    AcceleratedVideoEncoder::EncodeJob* job,
    const H264Encoder::EncodeParams& encode_params,
    const H264SPS& sps,
    const H264PPS& pps,
    scoped_refptr<H264Picture> pic,
    const std::list<scoped_refptr<H264Picture>>& ref_pic_list0,
    const std::list<scoped_refptr<H264Picture>>& ref_pic_list1) {
  VAEncSequenceParameterBufferH264 seq_param = {};

#define SPS_TO_SP(a) seq_param.a = sps.a;
  SPS_TO_SP(seq_parameter_set_id);
  SPS_TO_SP(level_idc);

  seq_param.intra_period = encode_params.i_period_frames;
  seq_param.intra_idr_period = encode_params.idr_period_frames;
  seq_param.ip_period = encode_params.ip_period_frames;
  seq_param.bits_per_second = encode_params.bitrate_bps;

  SPS_TO_SP(max_num_ref_frames);
  base::Optional<gfx::Size> coded_size = sps.GetCodedSize();
  if (!coded_size) {
    DVLOGF(1) << "Invalid coded size";
    return false;
  }
  constexpr int kH264MacroblockSizeInPixels = 16;
  seq_param.picture_width_in_mbs =
      coded_size->width() / kH264MacroblockSizeInPixels;
  seq_param.picture_height_in_mbs =
      coded_size->height() / kH264MacroblockSizeInPixels;

#define SPS_TO_SP_FS(a) seq_param.seq_fields.bits.a = sps.a;
  SPS_TO_SP_FS(chroma_format_idc);
  SPS_TO_SP_FS(frame_mbs_only_flag);
  SPS_TO_SP_FS(log2_max_frame_num_minus4);
  SPS_TO_SP_FS(pic_order_cnt_type);
  SPS_TO_SP_FS(log2_max_pic_order_cnt_lsb_minus4);
#undef SPS_TO_SP_FS

  SPS_TO_SP(bit_depth_luma_minus8);
  SPS_TO_SP(bit_depth_chroma_minus8);

  SPS_TO_SP(frame_cropping_flag);
  if (sps.frame_cropping_flag) {
    SPS_TO_SP(frame_crop_left_offset);
    SPS_TO_SP(frame_crop_right_offset);
    SPS_TO_SP(frame_crop_top_offset);
    SPS_TO_SP(frame_crop_bottom_offset);
  }

  SPS_TO_SP(vui_parameters_present_flag);
#define SPS_TO_SP_VF(a) seq_param.vui_fields.bits.a = sps.a;
  SPS_TO_SP_VF(timing_info_present_flag);
#undef SPS_TO_SP_VF
  SPS_TO_SP(num_units_in_tick);
  SPS_TO_SP(time_scale);
#undef SPS_TO_SP

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitBuffer,
                     base::Unretained(vea_), VAEncSequenceParameterBufferType,
                     MakeRefCountedBytes(&seq_param, sizeof(seq_param))));

  VAEncPictureParameterBufferH264 pic_param = {};

  auto va_surface_id = pic->AsVaapiH264Picture()->GetVASurfaceID();
  pic_param.CurrPic.picture_id = va_surface_id;
  pic_param.CurrPic.TopFieldOrderCnt = pic->top_field_order_cnt;
  pic_param.CurrPic.BottomFieldOrderCnt = pic->bottom_field_order_cnt;
  pic_param.CurrPic.flags = 0;

  pic_param.coded_buf = job->AsVaapiEncodeJob()->coded_buffer_id();
  pic_param.pic_parameter_set_id = pps.pic_parameter_set_id;
  pic_param.seq_parameter_set_id = pps.seq_parameter_set_id;
  pic_param.frame_num = pic->frame_num;
  pic_param.pic_init_qp = pps.pic_init_qp_minus26 + 26;
  pic_param.num_ref_idx_l0_active_minus1 =
      pps.num_ref_idx_l0_default_active_minus1;

  pic_param.pic_fields.bits.idr_pic_flag = pic->idr;
  pic_param.pic_fields.bits.reference_pic_flag = pic->ref;
#define PPS_TO_PP_PF(a) pic_param.pic_fields.bits.a = pps.a;
  PPS_TO_PP_PF(entropy_coding_mode_flag);
  PPS_TO_PP_PF(transform_8x8_mode_flag);
  PPS_TO_PP_PF(deblocking_filter_control_present_flag);
#undef PPS_TO_PP_PF

  VAEncSliceParameterBufferH264 slice_param = {};

  slice_param.num_macroblocks =
      seq_param.picture_width_in_mbs * seq_param.picture_height_in_mbs;
  slice_param.macroblock_info = VA_INVALID_ID;
  slice_param.slice_type = pic->type;
  slice_param.pic_parameter_set_id = pps.pic_parameter_set_id;
  slice_param.idr_pic_id = pic->idr_pic_id;
  slice_param.pic_order_cnt_lsb = pic->pic_order_cnt_lsb;
  slice_param.num_ref_idx_active_override_flag = true;

  for (size_t i = 0; i < base::size(pic_param.ReferenceFrames); ++i)
    InitVAPictureH264(&pic_param.ReferenceFrames[i]);

  for (size_t i = 0; i < base::size(slice_param.RefPicList0); ++i)
    InitVAPictureH264(&slice_param.RefPicList0[i]);

  for (size_t i = 0; i < base::size(slice_param.RefPicList1); ++i)
    InitVAPictureH264(&slice_param.RefPicList1[i]);

  VAPictureH264* ref_frames_entry = pic_param.ReferenceFrames;
  VAPictureH264* ref_list_entry = slice_param.RefPicList0;
  // Initialize the current entry on slice and picture reference lists to
  // |ref_pic| and advance list pointers.
  auto fill_ref_frame = [&ref_frames_entry,
                         &ref_list_entry](scoped_refptr<H264Picture> ref_pic) {
    VAPictureH264 va_pic_h264;
    InitVAPictureH264(&va_pic_h264);
    va_pic_h264.picture_id = ref_pic->AsVaapiH264Picture()->GetVASurfaceID();
    va_pic_h264.flags = 0;

    *ref_frames_entry = va_pic_h264;
    *ref_list_entry = va_pic_h264;
    ++ref_frames_entry;
    ++ref_list_entry;
  };

  // Fill slice_param.RefPicList{0,1} with pictures from ref_pic_list{0,1},
  // respectively, and pic_param.ReferenceFrames with entries from both.
  std::for_each(ref_pic_list0.begin(), ref_pic_list0.end(), fill_ref_frame);
  ref_list_entry = slice_param.RefPicList1;
  std::for_each(ref_pic_list1.begin(), ref_pic_list1.end(), fill_ref_frame);

  VAEncMiscParameterRateControl rate_control_param = {};
  rate_control_param.bits_per_second = encode_params.bitrate_bps;
  rate_control_param.target_percentage = kTargetBitratePercentage;
  rate_control_param.window_size = encode_params.cpb_window_size_ms;
  rate_control_param.initial_qp = pic_param.pic_init_qp;
  rate_control_param.rc_flags.bits.disable_frame_skip = true;

  VAEncMiscParameterFrameRate framerate_param = {};
  framerate_param.framerate = encode_params.framerate;

  VAEncMiscParameterHRD hrd_param = {};
  hrd_param.buffer_size = encode_params.cpb_size_bits;
  hrd_param.initial_buffer_fullness = hrd_param.buffer_size / 2;

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitBuffer,
                     base::Unretained(vea_), VAEncPictureParameterBufferType,
                     MakeRefCountedBytes(&pic_param, sizeof(pic_param))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitBuffer,
                     base::Unretained(vea_), VAEncSliceParameterBufferType,
                     MakeRefCountedBytes(&slice_param, sizeof(slice_param))));

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncodeAccelerator::SubmitVAEncMiscParamBuffer,
      base::Unretained(vea_), VAEncMiscParameterTypeRateControl,
      MakeRefCountedBytes(&rate_control_param, sizeof(rate_control_param))));

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncodeAccelerator::SubmitVAEncMiscParamBuffer,
      base::Unretained(vea_), VAEncMiscParameterTypeFrameRate,
      MakeRefCountedBytes(&framerate_param, sizeof(framerate_param))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitVAEncMiscParamBuffer,
                     base::Unretained(vea_), VAEncMiscParameterTypeHRD,
                     MakeRefCountedBytes(&hrd_param, sizeof(hrd_param))));

  return true;
}

scoped_refptr<H264Picture>
VaapiVideoEncodeAccelerator::H264Accelerator::GetPicture(
    AcceleratedVideoEncoder::EncodeJob* job) {
  return base::MakeRefCounted<VaapiH264Picture>(
      job->AsVaapiEncodeJob()->reconstructed_surface());
}

bool VaapiVideoEncodeAccelerator::H264Accelerator::SubmitPackedHeaders(
    AcceleratedVideoEncoder::EncodeJob* job,
    scoped_refptr<H264BitstreamBuffer> packed_sps,
    scoped_refptr<H264BitstreamBuffer> packed_pps) {
  // Submit SPS.
  VAEncPackedHeaderParameterBuffer par_buffer = {};
  par_buffer.type = VAEncPackedHeaderSequence;
  par_buffer.bit_length = packed_sps->BytesInBuffer() * 8;

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncodeAccelerator::SubmitBuffer, base::Unretained(vea_),
      VAEncPackedHeaderParameterBufferType,
      MakeRefCountedBytes(&par_buffer, sizeof(par_buffer))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitH264BitstreamBuffer,
                     base::Unretained(vea_), packed_sps));

  // Submit PPS.
  par_buffer = {};
  par_buffer.type = VAEncPackedHeaderPicture;
  par_buffer.bit_length = packed_pps->BytesInBuffer() * 8;

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncodeAccelerator::SubmitBuffer, base::Unretained(vea_),
      VAEncPackedHeaderParameterBufferType,
      MakeRefCountedBytes(&par_buffer, sizeof(par_buffer))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitH264BitstreamBuffer,
                     base::Unretained(vea_), packed_pps));

  return true;
}

scoped_refptr<VP8Picture>
VaapiVideoEncodeAccelerator::VP8Accelerator::GetPicture(
    AcceleratedVideoEncoder::EncodeJob* job) {
  return base::MakeRefCounted<VaapiVP8Picture>(
      job->AsVaapiEncodeJob()->reconstructed_surface());
}

bool VaapiVideoEncodeAccelerator::VP8Accelerator::SubmitFrameParameters(
    AcceleratedVideoEncoder::EncodeJob* job,
    const VP8Encoder::EncodeParams& encode_params,
    scoped_refptr<VP8Picture> pic,
    const Vp8ReferenceFrameVector& ref_frames) {
  VAEncSequenceParameterBufferVP8 seq_param = {};

  const auto& frame_header = pic->frame_hdr;
  seq_param.frame_width = frame_header->width;
  seq_param.frame_height = frame_header->height;
  seq_param.frame_width_scale = frame_header->horizontal_scale;
  seq_param.frame_height_scale = frame_header->vertical_scale;
  seq_param.error_resilient = 1;
  seq_param.bits_per_second = encode_params.bitrate_allocation.GetSumBps();
  seq_param.intra_period = encode_params.kf_period_frames;

  VAEncPictureParameterBufferVP8 pic_param = {};

  pic_param.reconstructed_frame = pic->AsVaapiVP8Picture()->GetVASurfaceID();
  DCHECK_NE(pic_param.reconstructed_frame, VA_INVALID_ID);

  auto last_frame = ref_frames.GetFrame(Vp8RefType::VP8_FRAME_LAST);
  pic_param.ref_last_frame =
      last_frame ? last_frame->AsVaapiVP8Picture()->GetVASurfaceID()
                 : VA_INVALID_ID;
  auto golden_frame = ref_frames.GetFrame(Vp8RefType::VP8_FRAME_GOLDEN);
  pic_param.ref_gf_frame =
      golden_frame ? golden_frame->AsVaapiVP8Picture()->GetVASurfaceID()
                   : VA_INVALID_ID;
  auto alt_frame = ref_frames.GetFrame(Vp8RefType::VP8_FRAME_ALTREF);
  pic_param.ref_arf_frame =
      alt_frame ? alt_frame->AsVaapiVP8Picture()->GetVASurfaceID()
                : VA_INVALID_ID;
  pic_param.coded_buf = job->AsVaapiEncodeJob()->coded_buffer_id();
  DCHECK_NE(pic_param.coded_buf, VA_INVALID_ID);

  if (frame_header->IsKeyframe())
    pic_param.ref_flags.bits.force_kf = true;

  pic_param.pic_flags.bits.frame_type = frame_header->frame_type;
  pic_param.pic_flags.bits.version = frame_header->version;
  pic_param.pic_flags.bits.show_frame = frame_header->show_frame;
  pic_param.pic_flags.bits.loop_filter_type = frame_header->loopfilter_hdr.type;
  pic_param.pic_flags.bits.num_token_partitions =
      frame_header->num_of_dct_partitions;
  pic_param.pic_flags.bits.segmentation_enabled =
      frame_header->segmentation_hdr.segmentation_enabled;
  pic_param.pic_flags.bits.update_mb_segmentation_map =
      frame_header->segmentation_hdr.update_mb_segmentation_map;
  pic_param.pic_flags.bits.update_segment_feature_data =
      frame_header->segmentation_hdr.update_segment_feature_data;

  pic_param.pic_flags.bits.loop_filter_adj_enable =
      frame_header->loopfilter_hdr.loop_filter_adj_enable;

  pic_param.pic_flags.bits.refresh_entropy_probs =
      frame_header->refresh_entropy_probs;
  pic_param.pic_flags.bits.refresh_golden_frame =
      frame_header->refresh_golden_frame;
  pic_param.pic_flags.bits.refresh_alternate_frame =
      frame_header->refresh_alternate_frame;
  pic_param.pic_flags.bits.refresh_last = frame_header->refresh_last;
  pic_param.pic_flags.bits.copy_buffer_to_golden =
      frame_header->copy_buffer_to_golden;
  pic_param.pic_flags.bits.copy_buffer_to_alternate =
      frame_header->copy_buffer_to_alternate;
  pic_param.pic_flags.bits.sign_bias_golden = frame_header->sign_bias_golden;
  pic_param.pic_flags.bits.sign_bias_alternate =
      frame_header->sign_bias_alternate;
  pic_param.pic_flags.bits.mb_no_coeff_skip = frame_header->mb_no_skip_coeff;
  if (frame_header->IsKeyframe())
    pic_param.pic_flags.bits.forced_lf_adjustment = true;

  static_assert(
      arraysize(pic_param.loop_filter_level) ==
              arraysize(pic_param.ref_lf_delta) &&
          arraysize(pic_param.ref_lf_delta) ==
              arraysize(pic_param.mode_lf_delta) &&
          arraysize(pic_param.ref_lf_delta) ==
              arraysize(frame_header->loopfilter_hdr.ref_frame_delta) &&
          arraysize(pic_param.mode_lf_delta) ==
              arraysize(frame_header->loopfilter_hdr.mb_mode_delta),
      "Invalid loop filter array sizes");

  for (size_t i = 0; i < base::size(pic_param.loop_filter_level); ++i) {
    pic_param.loop_filter_level[i] = frame_header->loopfilter_hdr.level;
    pic_param.ref_lf_delta[i] = frame_header->loopfilter_hdr.ref_frame_delta[i];
    pic_param.mode_lf_delta[i] = frame_header->loopfilter_hdr.mb_mode_delta[i];
  }

  pic_param.sharpness_level = frame_header->loopfilter_hdr.sharpness_level;
  pic_param.clamp_qindex_high = encode_params.max_qp;
  pic_param.clamp_qindex_low = encode_params.min_qp;

  VAQMatrixBufferVP8 qmatrix_buf = {};
  for (size_t i = 0; i < base::size(qmatrix_buf.quantization_index); ++i)
    qmatrix_buf.quantization_index[i] = frame_header->quantization_hdr.y_ac_qi;

  qmatrix_buf.quantization_index_delta[0] =
      frame_header->quantization_hdr.y_dc_delta;
  qmatrix_buf.quantization_index_delta[1] =
      frame_header->quantization_hdr.y2_dc_delta;
  qmatrix_buf.quantization_index_delta[2] =
      frame_header->quantization_hdr.y2_ac_delta;
  qmatrix_buf.quantization_index_delta[3] =
      frame_header->quantization_hdr.uv_dc_delta;
  qmatrix_buf.quantization_index_delta[4] =
      frame_header->quantization_hdr.uv_ac_delta;

  VAEncMiscParameterRateControl rate_control_param = {};
  rate_control_param.bits_per_second =
      encode_params.bitrate_allocation.GetSumBps();
  rate_control_param.target_percentage = kTargetBitratePercentage;
  rate_control_param.window_size = encode_params.cpb_window_size_ms;
  rate_control_param.initial_qp = encode_params.initial_qp;
  rate_control_param.rc_flags.bits.disable_frame_skip = true;

  VAEncMiscParameterFrameRate framerate_param = {};
  framerate_param.framerate = encode_params.framerate;

  VAEncMiscParameterHRD hrd_param = {};
  hrd_param.buffer_size = encode_params.cpb_size_bits;
  hrd_param.initial_buffer_fullness = hrd_param.buffer_size / 2;

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitBuffer,
                     base::Unretained(vea_), VAEncSequenceParameterBufferType,
                     MakeRefCountedBytes(&seq_param, sizeof(seq_param))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitBuffer,
                     base::Unretained(vea_), VAEncPictureParameterBufferType,
                     MakeRefCountedBytes(&pic_param, sizeof(pic_param))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitBuffer,
                     base::Unretained(vea_), VAQMatrixBufferType,
                     MakeRefCountedBytes(&qmatrix_buf, sizeof(qmatrix_buf))));

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncodeAccelerator::SubmitVAEncMiscParamBuffer,
      base::Unretained(vea_), VAEncMiscParameterTypeRateControl,
      MakeRefCountedBytes(&rate_control_param, sizeof(rate_control_param))));

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncodeAccelerator::SubmitVAEncMiscParamBuffer,
      base::Unretained(vea_), VAEncMiscParameterTypeFrameRate,
      MakeRefCountedBytes(&framerate_param, sizeof(framerate_param))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitVAEncMiscParamBuffer,
                     base::Unretained(vea_), VAEncMiscParameterTypeHRD,
                     MakeRefCountedBytes(&hrd_param, sizeof(hrd_param))));

  return true;
}

}  // namespace media
