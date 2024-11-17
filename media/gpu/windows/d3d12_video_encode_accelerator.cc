// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_video_encode_accelerator.h"

#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/thread_pool.h"
#include "media/gpu/macros.h"
#include "media/gpu/windows/d3d12_video_encode_delegate.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

namespace {
// Minimum number of frames in flight for pipeline depth, adjust to this number
// if encoder requests less. We assumes hardware encoding consists of 4 stages:
// motion estimation/compensation, transform/quantization, entropy coding and
// finally bitstream packing. So with this 4-stage pipeline it is expected at
// least 4 output bitstream buffer to be allocated for the encoder to operate
// properly.
constexpr size_t kMinNumFramesInFlight = 4;
}  // namespace

struct D3D12VideoEncodeAccelerator::InputFrameRef {
  InputFrameRef(scoped_refptr<VideoFrame> frame, bool force_keyframe)
      : frame(std::move(frame)), force_keyframe(force_keyframe) {}
  const scoped_refptr<VideoFrame> frame;
  const bool force_keyframe;
};

D3D12VideoEncodeAccelerator::D3D12VideoEncodeAccelerator(
    Microsoft::WRL::ComPtr<ID3D12Device> device)
    : device_(std::move(device)),
      child_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      encoder_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})) {
  DVLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);
  DETACH_FROM_SEQUENCE(encoder_sequence_checker_);

  // |video_device_| will be used by |GetSupportedProfiles()| before
  // |Initialize()| is called.
  CHECK(device_);
  // We will check and log error later in the Initialize().
  device_.As(&video_device_);

  child_weak_this_ = child_weak_this_factory_.GetWeakPtr();
  encoder_weak_this_ = encoder_weak_this_factory_.GetWeakPtr();

  encoder_info_.implementation_name = "D3D12VideoEncodeAccelerator";
  encoder_info_.reports_average_qp = false;
}

D3D12VideoEncodeAccelerator::~D3D12VideoEncodeAccelerator() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
}

VideoEncodeAccelerator::SupportedProfiles
D3D12VideoEncodeAccelerator::GetSupportedProfiles() {
  static const base::NoDestructor supported_profiles(
      [&]() -> SupportedProfiles {
        if (!video_device_) {
          return {};
        }
        if (encoder_factory_) {
          CHECK_IS_TEST();
          return encoder_factory_->GetSupportedProfiles(video_device_.Get());
        }
        return D3D12VideoEncodeDelegate::GetSupportedProfiles(
            video_device_.Get());
      }());
  return *supported_profiles.get();
}

bool D3D12VideoEncodeAccelerator::Initialize(
    const Config& config,
    Client* client,
    std::unique_ptr<MediaLog> media_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);
  VLOGF(2) << "Initializing D3D12VEA with config "
           << config.AsHumanReadableString();

  config_ = config;
  // A NV12 format frame consists of a Y-plane which occupies the same
  // size as the frame itself, and an UV-plane which is half the size
  // of the frame. Reserving a buffer of 1 + 1/2 = 3/2 times the size
  // of the frame bytes should be enough for a compressed bitstream.
  bitstream_buffer_size_ = config.input_visible_size.GetArea() * 3 / 2;
  client_ptr_factory_ = std::make_unique<base::WeakPtrFactory<Client>>(client);
  client_ = client_ptr_factory_->GetWeakPtr();
  media_log_ = std::move(media_log);

  if (!video_device_) {
    MEDIA_LOG(ERROR, media_log_) << "Failed to get D3D12 video device";
    return false;
  }

  if (config.HasSpatialLayer()) {
    MEDIA_LOG(ERROR, media_log_) << "Only L1T{1,2,3} mode is supported";
    return false;
  }

  error_occurred_ = false;
  encoder_task_runner_->PostTask(
      FROM_HERE, BindOnce(&D3D12VideoEncodeAccelerator::InitializeTask,
                          encoder_weak_this_, config));
  return true;
}

void D3D12VideoEncodeAccelerator::Encode(scoped_refptr<VideoFrame> frame,
                                         bool force_keyframe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);
  encoder_task_runner_->PostTask(
      FROM_HERE,
      BindOnce(&D3D12VideoEncodeAccelerator::EncodeTask, encoder_weak_this_,
               std::move(frame), force_keyframe));
}

void D3D12VideoEncodeAccelerator::UseOutputBitstreamBuffer(
    BitstreamBuffer buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);
  encoder_task_runner_->PostTask(
      FROM_HERE,
      BindOnce(&D3D12VideoEncodeAccelerator::UseOutputBitstreamBufferTask,
               encoder_weak_this_, std::move(buffer)));
}

void D3D12VideoEncodeAccelerator::RequestEncodingParametersChange(
    const Bitrate& bitrate,
    uint32_t framerate,
    const std::optional<gfx::Size>& size) {}

void D3D12VideoEncodeAccelerator::Destroy() {
  DVLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);

  child_weak_this_factory_.InvalidateWeakPtrs();

  // We're destroying; cancel all callbacks.
  if (client_ptr_factory_) {
    client_ptr_factory_->InvalidateWeakPtrs();
  }

  encoder_task_runner_->PostTask(
      FROM_HERE,
      BindOnce(&D3D12VideoEncodeAccelerator::DestroyTask, encoder_weak_this_));
}

void D3D12VideoEncodeAccelerator::InitializeTask(const Config& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  SupportedProfiles profiles = GetSupportedProfiles();
  auto profile = std::ranges::find(profiles, config.output_profile,
                                   &SupportedProfile::profile);
  if (profile == std::ranges::end(profiles)) {
    return NotifyError(
        {EncoderStatus::Codes::kEncoderUnsupportedProfile,
         base::StringPrintf("Unsupported output profile %s",
                            GetProfileName(config.output_profile))});
  }

  if (config.input_visible_size.width() > profile->max_resolution.width() ||
      config.input_visible_size.height() > profile->max_resolution.height() ||
      config.input_visible_size.width() < profile->min_resolution.width() ||
      config.input_visible_size.height() < profile->min_resolution.height()) {
    return NotifyError(
        {EncoderStatus::Codes::kEncoderUnsupportedConfig,
         base::StringPrintf(
             "Unsupported resolution: %s, supported resolution: %s to %s",
             config.input_visible_size.ToString(),
             profile->min_resolution.ToString(),
             profile->max_resolution.ToString())});
  }

  if (encoder_factory_) {
    CHECK_IS_TEST();
    encoder_ = encoder_factory_->CreateVideoEncodeDelegate(
        video_device_.Get(), config.output_profile);
  } else {
    // TODO(crbug.com/40275246): encoder_ will be initialized here.
  }

  if (EncoderStatus status = encoder_->Initialize(config); !status.is_ok()) {
    return NotifyError(status);
  }

  num_frames_in_flight_ =
      kMinNumFramesInFlight + encoder_->GetMaxNumOfRefFrames();

  child_task_runner_->PostTask(
      FROM_HERE,
      BindOnce(&Client::RequireBitstreamBuffers, client_, num_frames_in_flight_,
               config.input_visible_size, bitstream_buffer_size_));

  // TODO(crbug.com/40275246): This needs to be populated when temporal layers
  // support is implemented.
  constexpr uint8_t kFullFramerate = 255;
  encoder_info_.fps_allocation[0] = {kFullFramerate};

  child_task_runner_->PostTask(
      FROM_HERE,
      BindOnce(&Client::NotifyEncoderInfoChange, client_, encoder_info_));
}

void D3D12VideoEncodeAccelerator::UseOutputBitstreamBufferTask(
    BitstreamBuffer buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  if (buffer.size() < bitstream_buffer_size_) {
    return NotifyError({EncoderStatus::Codes::kInvalidOutputBuffer,
                        "Bitstream buffer size is too small"});
  }

  bitstream_buffers_.push(std::move(buffer));
  if (!input_frames_queue_.empty()) {
    DoEncodeTask(input_frames_queue_.front().frame,
                 input_frames_queue_.front().force_keyframe,
                 bitstream_buffers_.front());
    input_frames_queue_.pop();
    bitstream_buffers_.pop();
  }
}

Microsoft::WRL::ComPtr<ID3D12Resource>
D3D12VideoEncodeAccelerator::CreateResourceForGpuMemoryBufferVideoFrame(
    const VideoFrame& frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  CHECK_EQ(frame.storage_type(), VideoFrame::STORAGE_GPU_MEMORY_BUFFER);

  gfx::GpuMemoryBufferHandle handle = frame.GetGpuMemoryBufferHandle();
  Microsoft::WRL::ComPtr<ID3D12Resource> input_texture;
  // TODO(40275246): cache the result
  HRESULT hr = device_->OpenSharedHandle(handle.dxgi_handle.Get(),
                                         IID_PPV_ARGS(&input_texture));
  if (FAILED(hr)) {
    NotifyError({EncoderStatus::Codes::kInvalidInputFrame,
                 "Failed to OpenSharedHandle for input_texture"});
    return nullptr;
  }

  return input_texture;
}

Microsoft::WRL::ComPtr<ID3D12Resource>
D3D12VideoEncodeAccelerator::CreateResourceForSharedMemoryVideoFrame(
    const VideoFrame& frame) {
  // TODO(crbug.com/40275246)
  NOTIMPLEMENTED();
  return nullptr;
}

void D3D12VideoEncodeAccelerator::EncodeTask(scoped_refptr<VideoFrame> frame,
                                             bool force_keyframe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  input_frames_queue_.push({frame, force_keyframe});
  if (!bitstream_buffers_.empty()) {
    DoEncodeTask(input_frames_queue_.front().frame,
                 input_frames_queue_.front().force_keyframe,
                 bitstream_buffers_.front());
    input_frames_queue_.pop();
    bitstream_buffers_.pop();
  }
}

void D3D12VideoEncodeAccelerator::DoEncodeTask(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe,
    const BitstreamBuffer& bitstream_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  Microsoft::WRL::ComPtr<ID3D12Resource> input_texture;
  if (frame->storage_type() == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    input_texture = CreateResourceForGpuMemoryBufferVideoFrame(*frame);
  } else if (frame->storage_type() == VideoFrame::STORAGE_SHMEM) {
    input_texture = CreateResourceForSharedMemoryVideoFrame(*frame);
  } else {
    return NotifyError({EncoderStatus::Codes::kInvalidInputFrame,
                        "Unsupported frame storage type for encoding"});
  }
  if (!input_texture) {
    return NotifyError({EncoderStatus::Codes::kInvalidInputFrame,
                        "Failed to create input_texture"});
  }

  auto result_or_error = encoder_->Encode(input_texture, 0, frame->ColorSpace(),
                                          bitstream_buffer, force_keyframe);
  if (!result_or_error.has_value()) {
    return NotifyError(std::move(result_or_error).error());
  }

  D3D12VideoEncodeDelegate::EncodeResult result =
      std::move(result_or_error).value();
  result.metadata_.timestamp = frame->timestamp();
  child_task_runner_->PostTask(
      FROM_HERE, BindOnce(&Client::BitstreamBufferReady, client_,
                          result.bitstream_buffer_id_, result.metadata_));
}

void D3D12VideoEncodeAccelerator::DestroyTask() {
  DVLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  delete this;
}

void D3D12VideoEncodeAccelerator::NotifyError(EncoderStatus status) {
  if (!child_task_runner_->RunsTasksInCurrentSequence()) {
    child_task_runner_->PostTask(
        FROM_HERE, BindOnce(&D3D12VideoEncodeAccelerator::NotifyError,
                            child_weak_this_, std::move(status)));
    return;
  }

  CHECK(!status.is_ok());
  MEDIA_LOG(ERROR, media_log_)
      << "D3D12VEA error " << static_cast<int32_t>(status.code()) << ": "
      << status.message();
  if (!error_occurred_) {
    if (client_) {
      client_->NotifyErrorStatus(status);
      client_ptr_factory_->InvalidateWeakPtrs();
    }
    error_occurred_ = true;
  }
}

}  // namespace media
