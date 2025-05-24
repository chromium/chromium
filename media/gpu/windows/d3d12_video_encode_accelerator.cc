// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_video_encode_accelerator.h"

#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/thread_pool.h"
#include "media/base/encoder_status.h"
#include "media/base/video_util.h"
#include "media/gpu/macros.h"
#include "media/gpu/windows/d3d12_video_encode_av1_delegate.h"
#include "media/gpu/windows/d3d12_video_encode_delegate.h"
#include "media/gpu/windows/d3d12_video_encode_h264_delegate.h"
#include "media/gpu/windows/format_utils.h"
#include "third_party/microsoft_dxheaders/src/include/directx/d3dx12_core.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
#include "media/gpu/windows/d3d12_video_encode_h265_delegate.h"
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

namespace media {

namespace {
// Minimum number of frames in flight for pipeline depth, adjust to this number
// if encoder requests less. We assumes hardware encoding consists of 4 stages:
// motion estimation/compensation, transform/quantization, entropy coding and
// finally bitstream packing. So with this 4-stage pipeline it is expected at
// least 4 output bitstream buffer to be allocated for the encoder to operate
// properly.
constexpr size_t kMinNumFramesInFlight = 4;

class VideoEncodeDelegateFactory
    : public D3D12VideoEncodeAccelerator::VideoEncodeDelegateFactoryInterface {
 public:
  std::unique_ptr<D3D12VideoEncodeDelegate> CreateVideoEncodeDelegate(
      ID3D12VideoDevice3* video_device,
      VideoCodecProfile profile) override {
    switch (VideoCodecProfileToVideoCodec(profile)) {
      case VideoCodec::kH264:
        return std::make_unique<D3D12VideoEncodeH264Delegate>(video_device);
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      case VideoCodec::kHEVC:
        return std::make_unique<D3D12VideoEncodeH265Delegate>(video_device);
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      case VideoCodec::kAV1:
        return std::make_unique<D3D12VideoEncodeAV1Delegate>(video_device);
      default:
        return nullptr;
    }
  }

  VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles(
      ID3D12VideoDevice3* video_device) override {
    return D3D12VideoEncodeDelegate::GetSupportedProfiles(video_device);
  }
};
}  // namespace

struct D3D12VideoEncodeAccelerator::InputFrameRef {
  InputFrameRef(scoped_refptr<VideoFrame> frame,
                const VideoEncoder::EncodeOptions& options)
      : frame(std::move(frame)), options(options) {}
  const scoped_refptr<VideoFrame> frame;
  const VideoEncoder::EncodeOptions options;
};

D3D12VideoEncodeAccelerator::D3D12VideoEncodeAccelerator(
    Microsoft::WRL::ComPtr<ID3D12Device> device)
    : device_(std::move(device)),
      child_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      encoder_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
      encoder_factory_(std::make_unique<VideoEncodeDelegateFactory>()) {
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
}

D3D12VideoEncodeAccelerator::~D3D12VideoEncodeAccelerator() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
}

void D3D12VideoEncodeAccelerator::SetEncoderFactoryForTesting(
    std::unique_ptr<VideoEncodeDelegateFactoryInterface> encoder_factory) {
  encoder_factory_ = std::move(encoder_factory);
}

VideoEncodeAccelerator::SupportedProfiles
D3D12VideoEncodeAccelerator::GetSupportedProfiles() {
  static const base::NoDestructor supported_profiles(
      [&]() -> SupportedProfiles {
        if (!video_device_) {
          return {};
        }
        return encoder_factory_->GetSupportedProfiles(video_device_.Get());
      }());
  return *supported_profiles.get();
}

EncoderStatus D3D12VideoEncodeAccelerator::Initialize(
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
    return {EncoderStatus::Codes::kEncoderInitializationError};
  }

  if (config.HasSpatialLayer() || config.HasTemporalLayer()) {
    MEDIA_LOG(ERROR, media_log_) << "Only L1T1 mode is supported";
    return {EncoderStatus::Codes::kEncoderInitializationError};
  }

  SupportedProfiles profiles = GetSupportedProfiles();
  auto profile = std::ranges::find(profiles, config.output_profile,
                                   &SupportedProfile::profile);
  if (profile == std::ranges::end(profiles)) {
    MEDIA_LOG(ERROR, media_log_) << "Unsupported output profile "
                                 << GetProfileName(config.output_profile);
    return {EncoderStatus::Codes::kEncoderUnsupportedProfile};
  }

  if (config.input_visible_size.width() > profile->max_resolution.width() ||
      config.input_visible_size.height() > profile->max_resolution.height() ||
      config.input_visible_size.width() < profile->min_resolution.width() ||
      config.input_visible_size.height() < profile->min_resolution.height()) {
    MEDIA_LOG(ERROR, media_log_)
        << "Unsupported resolution: " << config.input_visible_size.ToString()
        << ", supported resolution: " << profile->min_resolution.ToString()
        << " to " << profile->max_resolution.ToString();
    return {EncoderStatus::Codes::kEncoderUnsupportedConfig};
  }

  error_occurred_ = false;
  encoder_task_runner_->PostTask(
      FROM_HERE, BindOnce(&D3D12VideoEncodeAccelerator::InitializeTask,
                          encoder_weak_this_, config));
  return {EncoderStatus::Codes::kOk};
}

void D3D12VideoEncodeAccelerator::Encode(scoped_refptr<VideoFrame> frame,
                                         bool force_keyframe) {
  Encode(std::move(frame), VideoEncoder::EncodeOptions(force_keyframe));
}

void D3D12VideoEncodeAccelerator::Encode(
    scoped_refptr<VideoFrame> frame,
    const VideoEncoder::EncodeOptions& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);
  encoder_task_runner_->PostTask(
      FROM_HERE, BindOnce(&D3D12VideoEncodeAccelerator::EncodeTask,
                          encoder_weak_this_, std::move(frame), options));
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
    const std::optional<gfx::Size>& size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);
  encoder_task_runner_->PostTask(
      FROM_HERE,
      BindOnce(
          &D3D12VideoEncodeAccelerator::RequestEncodingParametersChangeTask,
          encoder_weak_this_, bitrate, framerate, size));
}

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

base::SingleThreadTaskRunner*
D3D12VideoEncodeAccelerator::GetEncoderTaskRunnerForTesting() const {
  return encoder_task_runner_.get();
}

size_t D3D12VideoEncodeAccelerator::GetInputFramesQueueSizeForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  return input_frames_queue_.size();
}

size_t D3D12VideoEncodeAccelerator::GetBitstreamBuffersSizeForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  return bitstream_buffers_.size();
}

void D3D12VideoEncodeAccelerator::InitializeTask(const Config& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  copy_command_queue_ = D3D12CopyCommandQueueWrapper::Create(device_.Get());
  if (!copy_command_queue_) {
    return NotifyError({EncoderStatus::Codes::kSystemAPICallError,
                        "Failed to create D3D12CopyCommandQueueWrapper"});
  }

  encoder_ = encoder_factory_->CreateVideoEncodeDelegate(video_device_.Get(),
                                                         config.output_profile);
  if (!encoder_) {
    return NotifyError(EncoderStatus::Codes::kEncoderUnsupportedCodec);
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
  encoder_info_.reports_average_qp = encoder_->ReportsAverageQp();

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
                 input_frames_queue_.front().options,
                 bitstream_buffers_.front());
    input_frames_queue_.pop();
    bitstream_buffers_.pop();
  }
}

void D3D12VideoEncodeAccelerator::RequestEncodingParametersChangeTask(
    const Bitrate& bitrate,
    uint32_t framerate,
    const std::optional<gfx::Size>& size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  if (size.has_value()) {
    return NotifyError({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                        "Update output frame size is not supported"});
  }

  if (!encoder_->UpdateRateControl(bitrate, framerate)) {
    VLOGF(1) << "Failed to update bitrate " << bitrate.ToString()
             << " and framerate " << framerate;
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
  HRESULT hr = device_->OpenSharedHandle(handle.dxgi_handle().buffer_handle(),
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  if (frame.storage_type() != VideoFrame::STORAGE_SHMEM &&
      frame.storage_type() != VideoFrame::STORAGE_UNOWNED_MEMORY) {
    LOG(ERROR) << "Unsupported frame storage type for mapping";
    return nullptr;
  }
  CHECK(frame.IsMappable());

  D3D12_RESOURCE_DESC input_texture_desc = CD3DX12_RESOURCE_DESC::Tex2D(
      DXGI_FORMAT_NV12, config_.input_visible_size.width(),
      config_.input_visible_size.height(), 1, 1);
  Microsoft::WRL::ComPtr<ID3D12Resource> input_texture;
  HRESULT hr = device_->CreateCommittedResource(
      &D3D12HeapProperties::kDefault, D3D12_HEAP_FLAG_NONE, &input_texture_desc,
      D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&input_texture));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to CreateCommittedResource for input_texture";
    return nullptr;
  }

  gfx::Size y_size = VideoFrame::PlaneSize(
      PIXEL_FORMAT_NV12, VideoFrame::Plane::kY, config_.input_visible_size);
  gfx::Size uv_size = VideoFrame::PlaneSize(
      PIXEL_FORMAT_NV12, VideoFrame::Plane::kUV, config_.input_visible_size);
  uint32_t uv_offset = y_size.GetArea();

  D3D12_RESOURCE_DESC upload_buffer_desc =
      CD3DX12_RESOURCE_DESC::Buffer(uv_offset + uv_size.GetArea());
  Microsoft::WRL::ComPtr<ID3D12Resource> upload_buffer;
  hr = device_->CreateCommittedResource(
      &D3D12HeapProperties::kUpload, D3D12_HEAP_FLAG_NONE, &upload_buffer_desc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload_buffer));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to CreateCommittedResource for upload_buffer";
    return nullptr;
  }

  {
    ScopedD3D12ResourceMap map;
    if (!map.Map(upload_buffer.Get())) {
      LOG(ERROR) << "Failed to map upload_buffer";
      return nullptr;
    }
    scoped_refptr<VideoFrame> upload_frame = VideoFrame::WrapExternalYuvData(
        PIXEL_FORMAT_NV12, config_.input_visible_size,
        gfx::Rect(config_.input_visible_size), config_.input_visible_size,
        y_size.width(), uv_size.width(), map.data().first(uv_offset).data(),
        map.data().subspan(uv_offset).data(), frame.timestamp());
    EncoderStatus result =
        frame_converter_.ConvertAndScale(frame, *upload_frame);
    if (!result.is_ok()) {
      LOG(ERROR) << "Failed to ConvertAndScale frame: " << result.message();
      return nullptr;
    }
  }

  copy_command_queue_->CopyBufferToNV12Texture(
      input_texture.Get(), upload_buffer.Get(), 0, y_size.width(), uv_offset,
      uv_size.width());

  // TODO(crbug.com/382316466): Let command queue wait on the GPU
  if (!copy_command_queue_->ExecuteAndWait()) {
    LOG(ERROR) << "Failed to ExecuteAndWait copy_command_list";
    return nullptr;
  }

  return input_texture;
}

void D3D12VideoEncodeAccelerator::EncodeTask(
    scoped_refptr<VideoFrame> frame,
    const VideoEncoder::EncodeOptions& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  input_frames_queue_.push({frame, options});
  if (!bitstream_buffers_.empty()) {
    DoEncodeTask(input_frames_queue_.front().frame,
                 input_frames_queue_.front().options,
                 bitstream_buffers_.front());
    input_frames_queue_.pop();
    bitstream_buffers_.pop();
  }
}

void D3D12VideoEncodeAccelerator::DoEncodeTask(
    scoped_refptr<VideoFrame> frame,
    const VideoEncoder::EncodeOptions& options,
    const BitstreamBuffer& bitstream_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  Microsoft::WRL::ComPtr<ID3D12Resource> input_texture;
  if (frame->storage_type() == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    if (frame->HasNativeGpuMemoryBuffer()) {
      input_texture = CreateResourceForGpuMemoryBufferVideoFrame(*frame);
    } else {
      frame = ConvertToMemoryMappedFrame(std::move(frame));
      if (!frame) {
        return NotifyError(
            {EncoderStatus::Codes::kInvalidInputFrame,
             "Failed to convert shared memory GMB for encoding"});
      }
      input_texture = CreateResourceForSharedMemoryVideoFrame(*frame);
    }
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
                                          bitstream_buffer, options);
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
