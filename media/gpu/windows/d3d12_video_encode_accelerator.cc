// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_video_encode_accelerator.h"

#include <d3d11.h>

#include <algorithm>

#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/win/scoped_handle.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/ipc/common/dxgi_helpers.h"
#include "gpu/ipc/service/shared_image_stub.h"
#include "media/base/encoder_status.h"
#include "media/base/media_switches.h"
#include "media/base/video_util.h"
#include "media/gpu/command_buffer_helper.h"
#include "media/gpu/macros.h"
#include "media/gpu/windows/d3d12_video_encode_av1_delegate.h"
#include "media/gpu/windows/d3d12_video_encode_delegate.h"
#include "media/gpu/windows/d3d12_video_encode_h264_delegate.h"
#include "media/gpu/windows/format_utils.h"
#include "third_party/microsoft_dxheaders/src/include/directx/d3dx12_core.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

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

#define RETURN_ON_FAILURE_WITH_CALLBACK(hr, message)                       \
  if (FAILED(hr)) {                                                        \
    LOG(ERROR) << message << ": " << logging::SystemErrorCodeToString(hr); \
    std::move(frame_available_cb).Run(std::move(frame), nullptr, hr);      \
    return;                                                                \
  }

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
      ID3D12VideoDevice3* video_device,
      const std::vector<D3D12_VIDEO_ENCODER_CODEC>& codecs) override {
    return D3D12VideoEncodeDelegate::GetSupportedProfiles(video_device, codecs);
  }
};
}  // namespace

struct D3D12VideoEncodeAccelerator::InputFrameRef {
  InputFrameRef(scoped_refptr<VideoFrame> frame,
                const VideoEncoder::EncodeOptions& options,
                bool resolving_shared_image)
      : frame(std::move(frame)),
        options(options),
        resolving_shared_image(resolving_shared_image) {}
  const scoped_refptr<VideoFrame> frame;
  const VideoEncoder::EncodeOptions options;
  bool resolve_shared_image_requested = false;
  bool resolving_shared_image = false;
  gpu::Mailbox shared_image_token;
  Microsoft::WRL::ComPtr<ID3D12Resource> resolved_resource;
};

void GenerateResourceOnSynTokenReleased(
    scoped_refptr<VideoFrame> frame,
    Microsoft::WRL::ComPtr<ID3D12Device> d3d12_device,
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
    scoped_refptr<CommandBufferHelper> command_buffer_helper,
    FrameAvailableCB frame_available_cb) {
  gpu::SharedImageManager* shared_image_manager =
      command_buffer_helper->GetSharedImageManager();
  std::unique_ptr<gpu::VideoImageRepresentation> representation =
      shared_image_manager->ProduceVideo(
          d3d11_device, frame->shared_image()->mailbox(),
          command_buffer_helper->GetMemoryTypeTracker());
  auto scoped_read_access = representation->BeginScopedReadAccess();
  Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture =
      scoped_read_access->GetD3D11Texture();

  Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
  HRESULT hr = input_texture.As(&dxgi_resource);
  RETURN_ON_FAILURE_WITH_CALLBACK(
      hr, "Failed to query IDXGIResource1 from input texture.");

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_context;
  d3d11_device->GetImmediateContext(&d3d11_context);
  Microsoft::WRL::ComPtr<IDXGIDevice2> dxgi_device2;
  hr = d3d11_device.As(&dxgi_device2);
  RETURN_ON_FAILURE_WITH_CALLBACK(
      hr, "Failed to query IDXGIDevice2 from D3D11 device");

  base::win::ScopedHandle shared_handle;
  HANDLE input_handle = nullptr;
  hr = dxgi_resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ,
                                         nullptr, &input_handle);
  bool use_shared_handle = false;
  if (SUCCEEDED(hr)) {
    use_shared_handle = true;
    shared_handle.Set(input_handle);
  }

  D3D11_TEXTURE2D_DESC desc;
  input_texture->GetDesc(&desc);
  bool input_has_keyed_mutex =
      desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex;
  std::unique_ptr<gpu::DXGIScopedReleaseKeyedMutex> scoped_keyed_mutex;

  // If the input_texture is backed by shared handle, BeginScopedReadAccess()
  // will automatically acquire the keyed mutex if it exists.
  if (!use_shared_handle && input_has_keyed_mutex) {
    hr = input_texture.As(&keyed_mutex);
    if (SUCCEEDED(hr)) {
      // Acquire the keyed mutex before using the texture in D3D12.
      hr = keyed_mutex->AcquireSync(0, INFINITE);
      RETURN_ON_FAILURE_WITH_CALLBACK(hr, "Failed to acquire keyed mutex");
      scoped_keyed_mutex =
          std::make_unique<gpu::DXGIScopedReleaseKeyedMutex>(keyed_mutex, 0);
    }
  }
  // Sync the input texture before we hand over to D3D12. Experiment shows
  // that if we merely rely on the keyed mutex, we get artifacts on the D3D12
  // encode output.
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  hr = dxgi_device2->EnqueueSetEvent(event.handle());
  if (SUCCEEDED(hr)) {
    event.Wait();
  } else {
    LOG(WARNING) << "Failed to set event: "
                 << logging::SystemErrorCodeToString(hr);
    d3d11_context->Flush();
  }

  if (!use_shared_handle) {
    // If shared handle creation fails, create a copy of the texture. This does
    // not need to be a keyed mutex texture, as we will make sure the copy is
    // finished before handing over to D3D12, and D3D11 will not touch it any
    // more.
    desc.MiscFlags =
        D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.MipLevels = 1;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> shared_texture;
    hr = d3d11_device->CreateTexture2D(&desc, nullptr, &shared_texture);
    RETURN_ON_FAILURE_WITH_CALLBACK(
        hr, "Failed to create shared texture for copying from shared image");

    d3d11_context->CopySubresourceRegion(shared_texture.Get(), 0, 0, 0, 0,
                                         input_texture.Get(), 0, nullptr);

    // TODO(https://crbug.com/40275246): Pass a shared D3D11 fence and wait
    // on D3D12 video processor command queue, or D3D12 video encoder queue,
    // depending on whether VP is needed, instead of waiting on D3D11.
    hr = dxgi_device2->EnqueueSetEvent(event.handle());
    if (SUCCEEDED(hr)) {
      event.Wait();
    } else {
      LOG(WARNING) << "Failed to set event: "
                   << logging::SystemErrorCodeToString(hr);
      d3d11_context->Flush();
    }

    hr = shared_texture.As(&dxgi_resource);
    RETURN_ON_FAILURE_WITH_CALLBACK(
        hr, "Failed to query DXGI resource from shared texture");

    HANDLE copied_handle = nullptr;
    hr = dxgi_resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ,
                                           nullptr, &copied_handle);
    RETURN_ON_FAILURE_WITH_CALLBACK(
        hr, "Failed to create shared handle from copied texture");

    shared_handle.Set(copied_handle);
  }

  Microsoft::WRL::ComPtr<ID3D12Resource> d3d12_texture;
  hr = d3d12_device->OpenSharedHandle(shared_handle.Get(),
                                      IID_PPV_ARGS(&d3d12_texture));
  RETURN_ON_FAILURE_WITH_CALLBACK(
      hr, "Failed to open shared handle for D3D12 resource");

  std::move(frame_available_cb)
      .Run(std::move(frame), std::move(d3d12_texture), hr);
}

void GenerateResourceFromSharedImageVideoFrame(
    scoped_refptr<VideoFrame> frame,
    Microsoft::WRL::ComPtr<ID3D12Device> d3d12_device,
    scoped_refptr<CommandBufferHelper> command_buffer_helper,
    FrameAvailableCB frame_available_cb) {
  if (!frame->HasSharedImage()) {
    std::move(frame_available_cb).Run(std::move(frame), nullptr, E_FAIL);
    return;
  }

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      command_buffer_helper->GetSharedImageStub()
          ->shared_context_state()
          ->GetD3D11Device();
  if (!d3d11_device) {
    std::move(frame_available_cb).Run(std::move(frame), nullptr, E_FAIL);
    return;
  }

  gpu::SyncToken acquire_sync_token = frame->acquire_sync_token();
  command_buffer_helper->WaitForSyncToken(
      acquire_sync_token,
      base::BindOnce(&GenerateResourceOnSynTokenReleased, std::move(frame),
                     d3d12_device, d3d11_device, command_buffer_helper,
                     std::move(frame_available_cb)));
}

D3D12VideoEncodeAccelerator::GetCommandBufferHelperResult::
    GetCommandBufferHelperResult() = default;
D3D12VideoEncodeAccelerator::GetCommandBufferHelperResult::
    GetCommandBufferHelperResult(const GetCommandBufferHelperResult& other) =
        default;
D3D12VideoEncodeAccelerator::GetCommandBufferHelperResult::
    ~GetCommandBufferHelperResult() = default;

D3D12VideoEncodeAccelerator::GetCommandBufferHelperResult
GetCommandBufferHelperOnGpuThread(
    base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()>
        get_command_buffer_helper_cb) {
  D3D12VideoEncodeAccelerator::GetCommandBufferHelperResult result;
  result.command_buffer_helper = get_command_buffer_helper_cb.Run();

  // For D3D12 VEA, the encoding device is always on the same adapter as
  // rendering device, so we don't check if the adapter is the same as the one
  // used by CommandBufferHelper. Also with D3D12 VEA, the D3D11 device is
  // always used on GPU main, so multi-thread protection is not needed for
  // it.
  return result;
}

D3D12VideoEncodeAccelerator::D3D12VideoEncodeAccelerator(
    Microsoft::WRL::ComPtr<ID3D12Device> device,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds)
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

  if (!gpu_workarounds.disable_d3d12_h264_encoding) {
    codecs_.push_back(D3D12_VIDEO_ENCODER_CODEC_H264);
  }
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  if (!gpu_workarounds.disable_d3d12_hevc_encoding) {
    codecs_.push_back(D3D12_VIDEO_ENCODER_CODEC_HEVC);
  }
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  codecs_.push_back(D3D12_VIDEO_ENCODER_CODEC_AV1);

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
        return encoder_factory_->GetSupportedProfiles(video_device_.Get(),
                                                      codecs_);
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
  encoder_info_.requested_resolution_alignment = 2;
  encoder_info_.apply_alignment_to_all_simulcast_layers = true;

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
  TryEncodeNextFrame();
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
        y_size.width(), uv_size.width(), map.data().first(uv_offset),
        map.data().subspan(uv_offset), frame.timestamp());
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
  if (!frame->HasMappableGpuBuffer() && frame->HasSharedImage()) {
    InputFrameRef input_frame(frame, options,
                              /*resolving_shared_image=*/true);
    input_frame.shared_image_token = frame->shared_image()->mailbox();
    input_frame.resolve_shared_image_requested = acquired_command_buffer_;
    input_frames_queue_.push_back(std::move(input_frame));

    if (acquired_command_buffer_) {
      // If we don't have a command buffer yet, we will resolve the shared image
      // later when the command buffer is available.
      gpu_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &GenerateResourceFromSharedImageVideoFrame, frame, device_,
              command_buffer_helper_,
              base::BindPostTask(
                  encoder_task_runner_,
                  base::BindOnce(
                      &D3D12VideoEncodeAccelerator::OnSharedImageResolved,
                      encoder_weak_this_))));
    }
    return;
  } else {
    input_frames_queue_.push_back(
        {std::move(frame), options, /*resolving_shared_image=*/false});
  }
  if (!bitstream_buffers_.empty()) {
    TryEncodeNextFrame();
  }
}

void D3D12VideoEncodeAccelerator::TryEncodeNextFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  if (input_frames_queue_.empty() || bitstream_buffers_.empty()) {
    return;
  }

  auto& next_input = input_frames_queue_.front();
  if (next_input.resolving_shared_image ||
      (!next_input.frame->HasMappableGpuBuffer() &&
       next_input.frame->HasSharedImage() && !next_input.resolved_resource)) {
    // D3D12 VEA encodes frames one-by-one, so we will not try following
    // frames.
    return;
  }

  DoEncodeTask(next_input.frame, next_input.resolved_resource,
               next_input.options, bitstream_buffers_.front());
  input_frames_queue_.pop_front();
  bitstream_buffers_.pop();
}

void D3D12VideoEncodeAccelerator::DoEncodeTask(
    scoped_refptr<VideoFrame> frame,
    Microsoft::WRL::ComPtr<ID3D12Resource> resolved_texture,
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
  } else if (!resolved_texture) {
    return NotifyError({EncoderStatus::Codes::kInvalidInputFrame,
                        "Unsupported frame storage type for encoding"});
  }
  if (!input_texture && !resolved_texture) {
    return NotifyError({EncoderStatus::Codes::kInvalidInputFrame,
                        "Failed to create input_texture"});
  }

  auto result_or_error =
      encoder_->Encode(resolved_texture ? resolved_texture : input_texture, 0,
                       frame->ColorSpace(), bitstream_buffer, options);
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

void D3D12VideoEncodeAccelerator::OnCommandBufferHelperAvailable(
    const GetCommandBufferHelperResult& result) {
  command_buffer_helper_ = result.command_buffer_helper;
  acquired_command_buffer_ = true;

  // Resolve frames in the queue that are waiting for command buffer
  // availability.
  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&D3D12VideoEncodeAccelerator::ResolveQueuedSharedImages,
                     encoder_weak_this_));
}

void D3D12VideoEncodeAccelerator::SetCommandBufferHelperCB(
    base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()>
        get_command_buffer_helper_cb,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner) {
  if (!base::FeatureList::IsEnabled(kD3D12SharedImageEncode)) {
    return;
  }

  gpu_task_runner_ = gpu_task_runner;
  gpu_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetCommandBufferHelperOnGpuThread,
                     get_command_buffer_helper_cb),
      base::BindOnce(
          &D3D12VideoEncodeAccelerator::OnCommandBufferHelperAvailable,
          child_weak_this_));
}

// This runs on the encoder task runner. It does not replace the original
// video frame. Instead it will attach a resolved ID3D12Resource to
// corresponding entry in the `input_frames_queue_`.
void D3D12VideoEncodeAccelerator::OnSharedImageResolved(
    scoped_refptr<VideoFrame> frame,
    Microsoft::WRL::ComPtr<ID3D12Resource> input_texture,
    HRESULT hr) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  if (FAILED(hr)) {
    MEDIA_LOG(ERROR, media_log_)
        << "Failed to resolve shared image for frame, error code: " << std::hex
        << hr;
    return NotifyError({EncoderStatus::Codes::kSystemAPICallError,
                        "Failed to resolve shared image"});
  }

  // Find the matching frame in the queue and update it.
  auto it = std::find_if(input_frames_queue_.begin(), input_frames_queue_.end(),
                         [&](InputFrameRef& input_frame) {
                           return input_frame.resolving_shared_image &&
                                  input_frame.shared_image_token ==
                                      frame->shared_image()->mailbox();
                         });

  if (it == input_frames_queue_.end()) {
    return NotifyError(
        {EncoderStatus::Codes::kInvalidInputFrame,
         "Failed to find input frame for resolved shared image"});
  }
  it->resolving_shared_image = false;
  it->resolved_resource = std::move(input_texture);

  // Check if we can encode the front frame now.
  TryEncodeNextFrame();
}

void D3D12VideoEncodeAccelerator::ResolveQueuedSharedImages() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  for (auto& input_frame : input_frames_queue_) {
    if (!input_frame.frame->HasMappableGpuBuffer() &&
        input_frame.frame->HasSharedImage() &&
        !input_frame.resolve_shared_image_requested) {
      input_frame.resolve_shared_image_requested = true;
      gpu_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &GenerateResourceFromSharedImageVideoFrame, input_frame.frame,
              device_, command_buffer_helper_,
              base::BindPostTask(
                  encoder_task_runner_,
                  base::BindOnce(
                      &D3D12VideoEncodeAccelerator::OnSharedImageResolved,
                      encoder_weak_this_))));
    }
  }
}

}  // namespace media
