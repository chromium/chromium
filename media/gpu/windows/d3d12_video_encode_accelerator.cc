// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_video_encode_accelerator.h"

#include <d3d11.h>

#include <algorithm>
#include <ranges>
#include <utility>

#include "base/bits.h"
#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
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
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "media/gpu/command_buffer_helper.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "media/gpu/macros.h"
#include "media/gpu/windows/d3d12_video_encode_av1_delegate.h"
#include "media/gpu/windows/d3d12_video_encode_delegate.h"
#include "media/gpu/windows/d3d12_video_encode_h264_delegate.h"
#include "media/gpu/windows/packed_yuv_utils.h"
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

// UMA histogram for tracking D3D12 VEA usage and success rate.
// kInit: Recorded every time VEA Initialize() is called.
// kSuccess: Recorded only when VEA initialization completes successfully.
// Success percentage = (kSuccess count / kInit count) * 100.
enum class VEAInitSuccessRate {
  kInit = 0,
  kSuccess = 1,
  kMaxValue = kSuccess,
};

constexpr std::string_view kInitSuccessRateHistogramPrefix =
    "Media.VideoEncoder.D3D12VEA.InitSuccessRate.";
constexpr std::string_view kEncoderStatusHistogramPrefix =
    "Media.VideoEncoder.D3D12VEA.EncodeStatus.";

std::string GetInitSuccessRateHistogramName(VideoCodecProfile profile) {
  return base::StrCat(
      {kInitSuccessRateHistogramPrefix,
       GetCodecNameForUMA(VideoCodecProfileToVideoCodec(profile))});
}

std::string GetEncoderStatusHistogramName(VideoCodecProfile profile) {
  return base::StrCat(
      {kEncoderStatusHistogramPrefix,
       GetCodecNameForUMA(VideoCodecProfileToVideoCodec(profile))});
}

// DXGI offers bi-planar formats only for 4:2:0; the 4:2:2 and 4:4:4 encoder
// inputs (Y210/Y410 for HEVC RExt, AYUV for AV1 High) are packed single-plane
// layouts. The two are uploaded differently: a bi-planar frame is converted
// straight into the mapped upload buffer, while a packed one is converted to
// the planar or bi-planar format of matching geometry first and then
// interleaved by DXGIFramePacker. See packed_yuv_utils.h.

// Returns the VideoPixelFormat matching bi-planar |encoder_input_format|, or
// PIXEL_FORMAT_UNKNOWN if it is not a bi-planar format handled here.
VideoPixelFormat GetBiPlanarUploadFormat(DXGI_FORMAT encoder_input_format) {
  switch (encoder_input_format) {
    case DXGI_FORMAT_NV12:  // 8 bit 4:2:0.
      return PIXEL_FORMAT_NV12;
    case DXGI_FORMAT_P010:  // 10 bit 4:2:0.
      return PIXEL_FORMAT_P010LE;
    default:
      return PIXEL_FORMAT_UNKNOWN;
  }
}

#define RETURN_ON_FAILURE_WITH_CALLBACK(hr, message)                       \
  if (FAILED(hr)) {                                                        \
    LOG(ERROR) << message << ": " << logging::SystemErrorCodeToString(hr); \
    std::move(frame_available_cb)                                          \
        .Run(std::move(frame), base::win::ScopedHandle(),                  \
             Microsoft::WRL::ComPtr<SharedImageReadLock>(), 0, hr);        \
    return;                                                                \
  }

class VideoEncodeDelegateFactory
    : public D3D12VideoEncodeAccelerator::VideoEncodeDelegateFactoryInterface {
 public:
  VideoEncodeDelegateFactory(
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds)
      : gpu_workarounds_(gpu_workarounds) {}

  std::unique_ptr<D3D12VideoEncodeDelegate> CreateVideoEncodeDelegate(
      ID3D12VideoDevice3* video_device,
      VideoCodecProfile profile) override {
    switch (VideoCodecProfileToVideoCodec(profile)) {
      case VideoCodec::kH264:
        return std::make_unique<D3D12VideoEncodeH264Delegate>(video_device,
                                                              gpu_workarounds_);
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      case VideoCodec::kHEVC:
        return std::make_unique<D3D12VideoEncodeH265Delegate>(video_device,
                                                              gpu_workarounds_);
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      case VideoCodec::kAV1:
        return std::make_unique<D3D12VideoEncodeAV1Delegate>(video_device,
                                                             gpu_workarounds_);
      default:
        return nullptr;
    }
  }

  VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles(
      ID3D12VideoDevice3* video_device,
      const std::vector<D3D12_VIDEO_ENCODER_CODEC>& codecs) override {
    return D3D12VideoEncodeDelegate::GetSupportedProfiles(
        video_device, gpu_workarounds_, codecs);
  }

 private:
  const gpu::GpuDriverBugWorkarounds gpu_workarounds_;
};

void GenerateResourceOnSynTokenReleased(
    scoped_refptr<VideoFrame> frame,
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
    D3D11FenceAndValue fence_and_value,
    scoped_refptr<CommandBufferHelper> command_buffer_helper,
    FrameAvailableCB frame_available_cb) {
  // ProduceVideo may go through GLTextureImageBacking which uses GL calls
  // (e.g. glGenTextures), so a GL context must be current to avoid operating
  // on a stale foreign context left current by a prior scheduler task.
  auto* shared_image_stub = command_buffer_helper->GetSharedImageStub();
  if (!shared_image_stub || !shared_image_stub->shared_context_state()) {
    RETURN_ON_FAILURE_WITH_CALLBACK(E_FAIL,
                                    "Failed to get shared context state");
  }
  auto shared_context_state = shared_image_stub->shared_context_state();
  if (!shared_context_state->MakeCurrent(nullptr, /*needs_gl=*/true)) {
    RETURN_ON_FAILURE_WITH_CALLBACK(E_FAIL, "Failed to make context current");
  }

  gpu::SharedImageManager* shared_image_manager =
      command_buffer_helper->GetSharedImageManager();
  auto tracker = std::make_unique<gpu::MemoryTypeTracker>(
      base::WrapRefCounted(shared_image_stub->memory_tracker()));
  std::unique_ptr<gpu::VideoImageRepresentation> representation =
      shared_image_manager->ProduceVideo(
          d3d11_device, frame->shared_image()->mailbox(), tracker.get());
  RETURN_ON_FAILURE_WITH_CALLBACK(representation ? S_OK : E_FAIL,
                                  "Failed to produce video");

  if (representation->size() != frame->coded_size()) {
    RETURN_ON_FAILURE_WITH_CALLBACK(E_FAIL, "SharedImage size mismatch");
  }

  auto scoped_read_access = representation->BeginScopedReadAccess();
  if (!scoped_read_access) {
    RETURN_ON_FAILURE_WITH_CALLBACK(E_FAIL, "Failed to begin read access");
  }
  Microsoft::WRL::ComPtr<SharedImageReadLock> si_lock =
      Microsoft::WRL::Make<SharedImageReadLock>(
          std::move(representation), std::move(scoped_read_access), frame,
          std::move(shared_context_state), std::move(tracker));
  if (!si_lock) {
    RETURN_ON_FAILURE_WITH_CALLBACK(E_OUTOFMEMORY,
                                    "Failed to create SharedImageReadLock");
  }
  gpu::D3D11TextureAndArrayIndex input_texture =
      si_lock->access()->GetD3D11Texture();

  D3D11_TEXTURE2D_DESC desc;
  input_texture.texture->GetDesc(&desc);
  bool is_texture_array = desc.ArraySize > 1;
  // Array index must be 0 if input is not texture array.
  CHECK(is_texture_array || !input_texture.array_index);

  Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
  HRESULT hr = input_texture.texture.As(&dxgi_resource);
  CHECK_EQ(hr, S_OK);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_context;
  d3d11_device->GetImmediateContext(&d3d11_context);
  Microsoft::WRL::ComPtr<IDXGIDevice2> dxgi_device2;
  hr = d3d11_device.As(&dxgi_device2);
  CHECK_EQ(hr, S_OK);

  base::win::ScopedHandle shared_handle;
  if (!is_texture_array) {
    HANDLE input_handle = nullptr;
    hr = dxgi_resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ,
                                           nullptr, &input_handle);
    if (SUCCEEDED(hr)) {
      shared_handle.Set(input_handle);
    }
  }

  if (!shared_handle.is_valid()) {
    // If the input_texture is backed by shared handle, BeginScopedReadAccess()
    // will automatically acquire the keyed mutex if it exists.
    std::unique_ptr<gpu::DXGIScopedReleaseKeyedMutex> scoped_keyed_mutex;
    if (desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) {
      Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex;
      hr = input_texture.texture.As(&keyed_mutex);
      if (SUCCEEDED(hr)) {
        // Acquire the keyed mutex before using the texture in D3D12.
        hr = keyed_mutex->AcquireSync(0, INFINITE);
        RETURN_ON_FAILURE_WITH_CALLBACK(hr, "Failed to acquire keyed mutex");
        scoped_keyed_mutex =
            std::make_unique<gpu::DXGIScopedReleaseKeyedMutex>(keyed_mutex, 0);
      }
    }

    // If shared handle creation fails or the texture is an array, create a copy
    // of the texture. This does not need to be a keyed mutex texture, as we
    // will make sure the copy is finished before handing over to D3D12, and
    // D3D11 will not touch it any more.
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
                                         input_texture.texture.Get(),
                                         input_texture.array_index, nullptr);

    hr = shared_texture.As(&dxgi_resource);
    CHECK_EQ(hr, S_OK);

    HANDLE copied_handle = nullptr;
    hr = dxgi_resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ,
                                           nullptr, &copied_handle);
    RETURN_ON_FAILURE_WITH_CALLBACK(
        hr, "Failed to create shared handle from copied texture");

    shared_handle.Set(copied_handle);
  }

  // This `fence_and_value` is for D3D11 -> D3D12 synchronization:
  // The D3D11 fence signals completion of all D3D11 operations on the input
  // texture, so the D3D12 command queue can wait on it before safely consuming
  // the texture.
  Microsoft::WRL::ComPtr<ID3D11DeviceContext4> context4;
  hr = d3d11_context.As(&context4);
  CHECK_EQ(hr, S_OK);
  hr = context4->Signal(fence_and_value.first.Get(), fence_and_value.second);
  RETURN_ON_FAILURE_WITH_CALLBACK(hr, "Failed to signal d3d11 fence");

  std::move(frame_available_cb)
      .Run(std::move(frame), std::move(shared_handle), std::move(si_lock),
           fence_and_value.second, S_OK);
}

void D3D12GenerateResourceFromSharedImageVideoFrame(
    scoped_refptr<VideoFrame> frame,
    D3D11FenceAndValue fence_and_value,
    scoped_refptr<CommandBufferHelper> command_buffer_helper,
    FrameAvailableCB frame_available_cb) {
  if (!frame->HasSharedImage()) {
    std::move(frame_available_cb)
        .Run(std::move(frame), base::win::ScopedHandle(),
             Microsoft::WRL::ComPtr<SharedImageReadLock>(), 0, E_FAIL);
    return;
  }

  auto* shared_image_stub = command_buffer_helper->GetSharedImageStub();
  if (!shared_image_stub || !shared_image_stub->shared_context_state()) {
    std::move(frame_available_cb)
        .Run(std::move(frame), base::win::ScopedHandle(),
             Microsoft::WRL::ComPtr<SharedImageReadLock>(), 0, E_FAIL);
    return;
  }

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      shared_image_stub->shared_context_state()->GetD3D11Device();
  if (!d3d11_device) {
    std::move(frame_available_cb)
        .Run(std::move(frame), base::win::ScopedHandle(),
             Microsoft::WRL::ComPtr<SharedImageReadLock>(), 0, E_FAIL);
    return;
  }

  gpu::SyncToken acquire_sync_token = frame->acquire_sync_token();
  command_buffer_helper->WaitForSyncToken(
      acquire_sync_token,
      base::BindOnce(&GenerateResourceOnSynTokenReleased, std::move(frame),
                     d3d11_device, fence_and_value, command_buffer_helper,
                     std::move(frame_available_cb)));
}

bool ProfileMatchesConfig(
    const VideoEncodeAccelerator::SupportedProfile& profile,
    const VideoEncodeAccelerator::Config& config) {
  if (profile.profile != config.output_profile) {
    return false;
  }
  if (profile.chroma_sampling.has_value() &&
      profile.chroma_sampling !=
          VideoPixelFormatToChromaSampling(config.input_format)) {
    return false;
  }
  if (profile.bit_depth.has_value() &&
      profile.bit_depth !=
          base::checked_cast<uint8_t>(BitDepth(config.input_format))) {
    return false;
  }
  return true;
}

bool IsVisibleRectOriginChromaAligned(const VideoFrame& frame) {
  switch (VideoPixelFormatToChromaSampling(frame.format())) {
    case VideoChromaSampling::k420:
      return frame.visible_rect().x() % 2 == 0 &&
             frame.visible_rect().y() % 2 == 0;
    case VideoChromaSampling::k422:
      return frame.visible_rect().x() % 2 == 0;
    // 4:4:4, luma-only and RGB formats have no subsampling.
    case VideoChromaSampling::k444:
    case VideoChromaSampling::k400:
    case VideoChromaSampling::kUnknown:
      return true;
  }
  NOTREACHED();
}

}  // namespace

BiPlanarUploadLayout GetBiPlanarUploadLayout(VideoPixelFormat format,
                                             const gfx::Size& size) {
  // PlaneSize() returns samples, not bytes, so compute the row counts and row
  // bytes via Rows() and RowBytes(), which are byte-based and therefore
  // correct for 10-bit formats.
  const size_t y_row_bytes =
      VideoFrame::RowBytes(VideoFrame::Plane::kY, format, size.width());
  const size_t uv_row_bytes =
      VideoFrame::RowBytes(VideoFrame::Plane::kUV, format, size.width());
  const size_t y_rows =
      VideoFrame::Rows(VideoFrame::Plane::kY, format, size.height());
  const size_t uv_rows =
      VideoFrame::Rows(VideoFrame::Plane::kUV, format, size.height());
  const size_t y_pitch = base::bits::AlignUp(
      y_row_bytes, size_t{D3D12_TEXTURE_DATA_PITCH_ALIGNMENT});
  const size_t uv_pitch = base::bits::AlignUp(
      uv_row_bytes, size_t{D3D12_TEXTURE_DATA_PITCH_ALIGNMENT});
  const size_t uv_offset = base::bits::AlignUp(
      y_pitch * y_rows, size_t{D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT});
  return {y_pitch, uv_pitch, uv_offset, uv_offset + uv_pitch * uv_rows};
}

struct D3D12VideoEncodeAccelerator::InputFrameRef {
  InputFrameRef(scoped_refptr<VideoFrame> frame,
                const VideoEncoder::EncodeOptions& options,
                bool resolving_shared_image)
      : frame(std::move(frame)),
        options(options),
        resolving_shared_image(resolving_shared_image) {}
  InputFrameRef(InputFrameRef&&) = default;
  InputFrameRef& operator=(InputFrameRef&&) = default;
  scoped_refptr<VideoFrame> frame;
  VideoEncoder::EncodeOptions options;
  bool resolve_shared_image_requested = false;
  bool resolving_shared_image = false;
  gpu::Mailbox shared_image_token;
  Microsoft::WRL::ComPtr<SharedImageReadLock> scoped_read_access;
  D3D12PictureBuffer resolved_picture;
  base::TimeTicks frame_encode_start_time = base::TimeTicks::Now();
};

D3D12VideoEncodeAccelerator::GetCommandBufferHelperResult::
    GetCommandBufferHelperResult() = default;
D3D12VideoEncodeAccelerator::GetCommandBufferHelperResult::
    GetCommandBufferHelperResult(GetCommandBufferHelperResult&& other) =
        default;
D3D12VideoEncodeAccelerator::GetCommandBufferHelperResult&
D3D12VideoEncodeAccelerator::GetCommandBufferHelperResult::operator=(
    GetCommandBufferHelperResult&& other) = default;
D3D12VideoEncodeAccelerator::GetCommandBufferHelperResult::
    ~GetCommandBufferHelperResult() = default;

std::unique_ptr<D3D11To12Fence> Create11On12InteropFence(
    ID3D12Device* d3d12_device,
    ID3D11Device* d3d11_device);

D3D12VideoEncodeAccelerator::GetCommandBufferHelperResult
GetCommandBufferHelperOnGpuThread(
    base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()>
        get_command_buffer_helper_cb,
    Microsoft::WRL::ComPtr<ID3D12Device> d3d12_device) {
  D3D12VideoEncodeAccelerator::GetCommandBufferHelperResult result;
  result.command_buffer_helper = get_command_buffer_helper_cb.Run();

  if (result.command_buffer_helper) {
    auto* shared_image_stub =
        result.command_buffer_helper->GetSharedImageStub();
    if (shared_image_stub && shared_image_stub->shared_context_state()) {
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
          shared_image_stub->shared_context_state()->GetD3D11Device();
      if (d3d11_device) {
        result.source_texture_fence =
            Create11On12InteropFence(d3d12_device.Get(), d3d11_device.Get());
      }
    }
  }

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
      encoder_factory_(
          std::make_unique<VideoEncodeDelegateFactory>(gpu_workarounds)),
      // VCM on Windows allocates 10 slots for MappableSI frames. Typically 3 of
      // them are being actively used. Set cache size to 5 to leave some room
      // for frames in the rendering and encoding pipelines.
      shared_handle_cache_(/*max_size=*/5) {
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
  if (!gpu_workarounds.disable_d3d12_av1_encoding) {
    codecs_.push_back(D3D12_VIDEO_ENCODER_CODEC_AV1);
  }

  child_weak_this_ = child_weak_this_factory_.GetWeakPtr();
  encoder_weak_this_ = encoder_weak_this_factory_.GetWeakPtr();

  encoder_info_.implementation_name = "D3D12VideoEncodeAccelerator";
}

D3D12VideoEncodeAccelerator::~D3D12VideoEncodeAccelerator() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  // Wait for any in-flight copy to complete before the members are destroyed,
  // so that the resources backing the copy (e.g. `upload_buffer_`,
  // `input_texture_`) are not released while the GPU may still be reading from
  // or writing to them.
  if (copy_command_queue_) {
    copy_command_queue_->WaitSync();
  }

  if (!error_occurred_ && encoded_at_least_one_frame_) {
    base::UmaHistogramEnumeration(
        GetEncoderStatusHistogramName(config_.output_profile),
        EncoderStatus::Codes::kOk);
  }
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
  base::UmaHistogramEnumeration(
      GetInitSuccessRateHistogramName(config.output_profile),
      VEAInitSuccessRate::kInit);

  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);
  VLOGF(2) << "Initializing D3D12VEA with config "
           << config.AsHumanReadableString();

  config_ = config;
  client_ptr_factory_ = std::make_unique<base::WeakPtrFactory<Client>>(client);
  client_ = client_ptr_factory_->GetWeakPtr();
  media_log_ = std::move(media_log);

  if (!video_device_) {
    MEDIA_LOG(ERROR, media_log_) << "Failed to get D3D12 video device";
    return {EncoderStatus::Codes::kEncoderInitializationError};
  }

  if (config.HasSpatialLayer()) {
    MEDIA_LOG(ERROR, media_log_)
        << "D3D12VideoEncodeAccelerator don't support spatial layers";
    return {EncoderStatus::Codes::kEncoderUnsupportedConfig};
  }
  uint8_t num_of_temporal_layers =
      config.spatial_layers.empty()
          ? 1
          : config.spatial_layers[0].num_of_temporal_layers;
  CHECK_GT(num_of_temporal_layers, 0u);
  if (num_of_temporal_layers > 3) {
    MEDIA_LOG(ERROR, media_log_) << base::StringPrintf(
        "D3D12VideoEncodeAccelerator don't support %u temporal layers",
        num_of_temporal_layers);
    return {EncoderStatus::Codes::kEncoderUnsupportedConfig};
  }

  SupportedProfiles profiles = GetSupportedProfiles();
  auto profile =
      std::ranges::find_if(profiles, [&config](const auto& candidate) {
        return ProfileMatchesConfig(candidate, config);
      });
  if (profile == std::ranges::end(profiles)) {
    MEDIA_LOG(ERROR, media_log_) << "Unsupported output profile "
                                 << GetProfileName(config.output_profile);
    return {EncoderStatus::Codes::kEncoderUnsupportedProfile};
  }
  SVCScalabilityMode scalability_mode = GetSVCScalabilityMode(
      1, num_of_temporal_layers, SVCInterLayerPredMode::kOff);
  if (std::ranges::find(profile->scalability_modes, scalability_mode) ==
      std::ranges::end(profile->scalability_modes)) {
    MEDIA_LOG(ERROR, media_log_)
        << base::StrCat({"Unsupported scalability mode ",
                         GetScalabilityModeName(scalability_mode)});
    return {EncoderStatus::Codes::kEncoderUnsupportedConfig};
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
                          encoder_weak_this_, config, std::move(profiles)));
  return EncoderStatus::Codes::kOk;
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
          encoder_weak_this_, BitrateToBitrateAllocation(bitrate), framerate,
          size));
}

void D3D12VideoEncodeAccelerator::RequestEncodingParametersChange(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate,
    const std::optional<gfx::Size>& size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);
  encoder_task_runner_->PostTask(
      FROM_HERE,
      BindOnce(
          &D3D12VideoEncodeAccelerator::RequestEncodingParametersChangeTask,
          encoder_weak_this_, bitrate_allocation, framerate, size));
}

void D3D12VideoEncodeAccelerator::Destroy() {
  DVLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);

  destroy_requested_ = true;
  child_weak_this_factory_.InvalidateWeakPtrsAndDoom();

  // We're destroying; cancel all callbacks.
  if (client_ptr_factory_) {
    client_ptr_factory_->InvalidateWeakPtrsAndDoom();
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

size_t D3D12VideoEncodeAccelerator::GetSharedHandleCacheSizeForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  return shared_handle_cache_.size();
}

void D3D12VideoEncodeAccelerator::InitializeTask(
    const Config& config,
    const SupportedProfiles& profiles) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  copy_command_queue_ = D3D12CopyCommandQueueWrapper::Create(device_.Get());
  if (!copy_command_queue_) {
    return NotifyError({EncoderStatus::Codes::kD3D12CreateCopyQueueFailed,
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

  // Take the size the delegate sized its bitstream buffer against, so the
  // capacity advertised to the client and the encoder's own buffer cannot
  // disagree.
  bitstream_buffer_size_ =
      base::checked_cast<size_t>(encoder_->GetMinBitstreamBufferSize());

  size_t num_of_manual_reference_buffers =
      encoder_->GetMaxNumOfManualRefBuffers();
  if (config.manual_reference_buffer_control &&
      num_of_manual_reference_buffers < 1) {
    return NotifyError({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                        "At least one manual reference buffer required."});
  }

  num_frames_in_flight_ =
      kMinNumFramesInFlight + encoder_->GetMaxNumOfRefFrames();

  // Set the fps allocation for the first spatial layer
  encoder_info_.fps_allocation[0] =
      GetFpsAllocation(encoder_->GetNumTemporalLayers());
  encoder_info_.reports_average_qp = encoder_->ReportsAverageQp();
  encoder_info_.requested_resolution_alignment = 2;
  encoder_info_.apply_alignment_to_all_simulcast_layers = true;
  encoder_info_.number_of_manual_reference_buffers =
      num_of_manual_reference_buffers;

  auto profile_it =
      std::ranges::find_if(profiles, [&config](const auto& candidate) {
        return ProfileMatchesConfig(candidate, config);
      });
  if (profile_it != std::ranges::end(profiles)) {
    encoder_info_.gpu_supported_pixel_formats =
        profile_it->gpu_supported_pixel_formats;
    encoder_info_.supports_gpu_shared_images =
        profile_it->supports_gpu_shared_images;
  } else {
    encoder_info_.supports_gpu_shared_images = false;
    encoder_info_.gpu_supported_pixel_formats.clear();
  }

  child_task_runner_->PostTask(
      FROM_HERE,
      BindOnce(&Client::NotifyEncoderInfoChange, client_, encoder_info_));

  // Must be called after NotifyEncoderInfoChange() to avoid readback
  // on first frame when shared image encoding is enabled. See
  // https://crbug.com/441011637 for more details.
  child_task_runner_->PostTask(
      FROM_HERE,
      BindOnce(&Client::RequireBitstreamBuffers, client_, num_frames_in_flight_,
               config.input_visible_size, bitstream_buffer_size_));

  metrics_helper_ = std::make_unique<VEAEncodingLatencyMetricsHelper>(
      "Media.VideoEncoder.D3D12VEA.EncodingLatency.",
      VideoCodecProfileToVideoCodec(config.output_profile));

  base::UmaHistogramEnumeration(
      GetInitSuccessRateHistogramName(config.output_profile),
      VEAInitSuccessRate::kSuccess);
}

void D3D12VideoEncodeAccelerator::UseOutputBitstreamBufferTask(
    BitstreamBuffer buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  if (buffer.size() < bitstream_buffer_size_) {
    return NotifyError({EncoderStatus::Codes::kInvalidOutputBuffer,
                        "Bitstream buffer size is too small"});
  }

  bitstream_buffers_.push(std::move(buffer));
  TryEncodeFrames();
}

void D3D12VideoEncodeAccelerator::RequestEncodingParametersChangeTask(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate,
    const std::optional<gfx::Size>& size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  if (size.has_value()) {
    return NotifyError({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                        "Update output frame size is not supported"});
  }

  if (!encoder_->UpdateRateControl(bitrate_allocation, framerate)) {
    VLOGF(1) << "Failed to update bitrate " << bitrate_allocation.ToString()
             << " and framerate " << framerate;
  }
}

D3D12PictureBuffer
D3D12VideoEncodeAccelerator::CreateResourceForDXGIHandleBackedVideoFrame(
    const VideoFrame& frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  CHECK(frame.HasMappableSharedImage());

  gfx::GpuMemoryBufferHandle handle = frame.GetGpuMemoryBufferHandle();
  Microsoft::WRL::ComPtr<ID3D12Resource> input_texture;

  static const bool caching_enabled = base::FeatureList::IsEnabled(
      kD3D12VideoEncodeAcceleratorSharedHandleCaching);

  const gfx::DXGIHandleToken& token = handle.dxgi_handle().token();
  if (caching_enabled) {
    // The Video Capture Module (VCM) reuses a small, circular pool of handles.
    // This means the same buffer handle will reappear periodically, but each
    // time it does, it will have been overwritten with a new frame's content
    // by the producer. When the encoder sees a handle it has seen before, it
    // retrieves the cached ID3D12Resource from the map. This resource object
    // is still a valid view into the same underlying GPU resource allocation.
    // When the GPU is instructed to use this resource for encoding, it reads
    // the current content of that memory, which is the new frame's data.
    auto cache_it = shared_handle_cache_.Get(token);
    if (cache_it != shared_handle_cache_.end()) {
      return cache_it->second;
    }
  }

  HRESULT hr = device_->OpenSharedHandle(handle.dxgi_handle().buffer_handle(),
                                         IID_PPV_ARGS(&input_texture));
  if (FAILED(hr)) {
    NotifyError({EncoderStatus::Codes::kInvalidInputFrame,
                 "Failed to OpenSharedHandle for input_texture"});
    return {};
  }

  if (caching_enabled) {
    shared_handle_cache_.Put(token, input_texture);
  }
  return input_texture;
}

bool D3D12VideoEncodeAccelerator::EnsureInputTexture(DXGI_FORMAT format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  D3D12_RESOURCE_DESC input_texture_desc =
      CD3DX12_RESOURCE_DESC::Tex2D(format, config_.input_visible_size.width(),
                                   config_.input_visible_size.height(), 1, 1);
  // The format is part of the reuse predicate: a cached texture of a different
  // format cannot receive this frame even when it is large enough.
  if (input_texture_ && input_texture_->GetDesc().Format == format &&
      input_texture_->GetDesc().Width >= input_texture_desc.Width &&
      input_texture_->GetDesc().Height >= input_texture_desc.Height) {
    return true;
  }
  HRESULT hr = device_->CreateCommittedResource(
      &D3D12HeapProperties::kDefault, D3D12_HEAP_FLAG_NONE, &input_texture_desc,
      D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&input_texture_));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to CreateCommittedResource for input_texture";
    return false;
  }
  std::wstring debug_name = base::UTF8ToWide(base::StringPrintf(
      "D3D12VEA input_texture_ %dx%d", config_.input_visible_size.width(),
      config_.input_visible_size.height()));
  CHECK_EQ(input_texture_->SetName(debug_name.c_str()), S_OK);
  return true;
}

bool D3D12VideoEncodeAccelerator::EnsureUploadBuffer(uint64_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  D3D12_RESOURCE_DESC upload_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(size);
  if (upload_buffer_ &&
      upload_buffer_->GetDesc().Width >= upload_buffer_desc.Width) {
    return true;
  }
  HRESULT hr = device_->CreateCommittedResource(
      &D3D12HeapProperties::kUpload, D3D12_HEAP_FLAG_NONE, &upload_buffer_desc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&upload_buffer_));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to CreateCommittedResource for upload_buffer";
    return false;
  }
  std::wstring debug_name = base::UTF8ToWide(base::StringPrintf(
      "D3D12VEA upload_buffer_ %dx%d", config_.input_visible_size.width(),
      config_.input_visible_size.height()));
  CHECK_EQ(upload_buffer_->SetName(debug_name.c_str()), S_OK);
  return true;
}

bool D3D12VideoEncodeAccelerator::UploadBiPlanarVideoFrame(
    const VideoFrame& frame,
    DXGI_FORMAT dxgi_format,
    VideoPixelFormat pixel_format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  if (!EnsureInputTexture(dxgi_format)) {
    return false;
  }

  const BiPlanarUploadLayout layout =
      GetBiPlanarUploadLayout(pixel_format, config_.input_visible_size);

  if (!EnsureUploadBuffer(layout.buffer_size)) {
    return false;
  }

  {
    ScopedD3D12ResourceMap map;
    if (!map.Map(upload_buffer_.Get())) {
      LOG(ERROR) << "Failed to map upload_buffer";
      return false;
    }
    scoped_refptr<VideoFrame> upload_frame = VideoFrame::WrapExternalYuvData(
        pixel_format, config_.input_visible_size,
        gfx::Rect(config_.input_visible_size), config_.input_visible_size,
        base::checked_cast<int>(layout.y_pitch),
        base::checked_cast<int>(layout.uv_pitch),
        map.data().first(layout.uv_offset),
        map.data().subspan(layout.uv_offset), frame.timestamp());
    EncoderStatus result =
        frame_converter_.ConvertAndScale(frame, *upload_frame);
    if (!result.is_ok()) {
      LOG(ERROR) << "Failed to ConvertAndScale frame: " << result.message();
      return false;
    }
  }

  if (!copy_command_queue_->CopyBufferToBiPlanarTexture(
          input_texture_.Get(), upload_buffer_.Get(),
          config_.input_visible_size, 0,
          base::checked_cast<uint32_t>(layout.y_pitch),
          base::checked_cast<uint32_t>(layout.uv_offset),
          base::checked_cast<uint32_t>(layout.uv_pitch))) {
    LOG(ERROR) << "Failed to CopyBufferToBiPlanarTexture";
    return false;
  }
  return true;
}

bool D3D12VideoEncodeAccelerator::UploadPackedVideoFrame(
    const VideoFrame& frame,
    DXGI_FORMAT dxgi_format,
    VideoPixelFormat source_format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  if (!EnsureInputTexture(dxgi_format)) {
    return false;
  }

  const size_t row_bytes =
      GetPackedDxgiRowBytes(dxgi_format, config_.input_visible_size.width());
  if (row_bytes == 0) {
    LOG(ERROR) << "Unsupported packed encoder input format";
    return false;
  }
  // CopyTextureRegion() reads the buffer as a placed footprint, whose row pitch
  // must be D3D12_TEXTURE_DATA_PITCH_ALIGNMENT aligned.
  const size_t row_pitch = base::bits::AlignUp(
      row_bytes, size_t{D3D12_TEXTURE_DATA_PITCH_ALIGNMENT});
  const size_t height =
      base::checked_cast<size_t>(config_.input_visible_size.height());
  const base::CheckedNumeric<size_t> buffer_size =
      base::CheckMul(row_pitch, height);
  if (!buffer_size.IsValid()) {
    LOG(ERROR) << "Upload buffer size overflowed";
    return false;
  }
  if (!EnsureUploadBuffer(buffer_size.ValueOrDie())) {
    return false;
  }

  // DXGIFramePacker interleaves from a planar or bi-planar frame, which is
  // what VideoFrameConverter can produce. A frame already in the source
  // format and the target geometry is packed directly; otherwise convert
  // into the staging frame, which is retained across frames to keep the
  // encode path free of per-frame allocations. Compare against the visible
  // size rather than the coded size, which VideoFrame pads to the format's
  // sample boundaries.
  const VideoFrame* pack_source_frame = &frame;
  if (frame.format() != source_format ||
      frame.visible_rect().size() != config_.input_visible_size) {
    if (!packing_source_frame_ ||
        packing_source_frame_->format() != source_format ||
        packing_source_frame_->visible_rect().size() !=
            config_.input_visible_size) {
      packing_source_frame_ = VideoFrame::CreateZeroInitializedFrame(
          source_format, config_.input_visible_size,
          gfx::Rect(config_.input_visible_size), config_.input_visible_size,
          base::TimeDelta());
      if (!packing_source_frame_) {
        LOG(ERROR) << "Failed to allocate packing source frame";
        return false;
      }
    }

    EncoderStatus result =
        frame_converter_.ConvertAndScale(frame, *packing_source_frame_);
    if (!result.is_ok()) {
      LOG(ERROR) << "Failed to ConvertAndScale frame: " << result.message();
      return false;
    }
    pack_source_frame = packing_source_frame_.get();
  }

  {
    ScopedD3D12ResourceMap map;
    if (!map.Map(upload_buffer_.Get())) {
      LOG(ERROR) << "Failed to map upload_buffer";
      return false;
    }
    if (!packed_dxgi_packer_.Pack(*pack_source_frame, dxgi_format, map.data(),
                                  row_pitch)) {
      LOG(ERROR) << "Failed to pack frame into the encoder input format";
      return false;
    }
  }

  if (!copy_command_queue_->CopyTextureRegion(
          {.pResource = input_texture_.Get(),
           .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
           .SubresourceIndex = 0},
          0, 0, 0,
          {.pResource = upload_buffer_.Get(),
           .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
           .PlacedFootprint = {
               .Offset = 0,
               .Footprint = {
                   .Format = dxgi_format,
                   .Width = base::checked_cast<UINT>(
                       config_.input_visible_size.width()),
                   .Height = base::checked_cast<UINT>(
                       config_.input_visible_size.height()),
                   .Depth = 1,
                   .RowPitch = base::checked_cast<UINT>(row_pitch)}}})) {
    LOG(ERROR) << "Failed to copy packed frame into input_texture";
    return false;
  }
  return true;
}

D3D12PictureBuffer
D3D12VideoEncodeAccelerator::CreateResourceForSharedMemoryVideoFrame(
    const VideoFrame& frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  if (frame.storage_type() != VideoFrame::STORAGE_SHMEM &&
      frame.storage_type() != VideoFrame::STORAGE_UNOWNED_MEMORY) {
    LOG(ERROR) << "Unsupported frame storage type for mapping";
    return {};
  }
  CHECK(frame.HasDirectCpuAccess());
  if (!IsVisibleRectOriginChromaAligned(frame)) {
    LOG(ERROR) << "Frame visible rect origin is not aligned to the chroma "
                  "subsampling of its format";
    return {};
  }

  // Upload in the encoder's own input format so the delegate's video processor
  // pass is skipped entirely, and so packed 4:2:2/4:4:4 inputs keep their
  // chroma resolution instead of going through a 4:2:0 intermediate.
  const DXGI_FORMAT dxgi_format = encoder_->GetInputFormat();
  bool uploaded = false;
  if (const VideoPixelFormat packed_source_format =
          GetPackedDxgiSourceFormat(dxgi_format);
      packed_source_format != PIXEL_FORMAT_UNKNOWN) {
    uploaded = UploadPackedVideoFrame(frame, dxgi_format, packed_source_format);
  } else if (const VideoPixelFormat bi_planar_format =
                 GetBiPlanarUploadFormat(dxgi_format);
             bi_planar_format != PIXEL_FORMAT_UNKNOWN) {
    uploaded = UploadBiPlanarVideoFrame(frame, dxgi_format, bi_planar_format);
  } else {
    LOG(ERROR) << "Unsupported encoder input format for shared memory upload";
    return {};
  }
  if (!uploaded) {
    return {};
  }

  auto fence_and_value = copy_command_queue_->Execute();
  if (!fence_and_value.first) {
    LOG(ERROR) << "Failed to Execute on copy_command_list";
    return {};
  }

  return {input_texture_, 0, fence_and_value};
}

void D3D12VideoEncodeAccelerator::EncodeTask(
    scoped_refptr<VideoFrame> frame,
    const VideoEncoder::EncodeOptions& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  if (!frame->HasMappableSharedImage() && frame->HasSharedImage()) {
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
              &D3D12GenerateResourceFromSharedImageVideoFrame, frame,
              source_texture_fence_->GetD3D11FenceAndIncrementValue(),
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
    TryEncodeFrames();
  }
}

void D3D12VideoEncodeAccelerator::TryEncodeFrames() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  if (error_occurred_) {
    return;
  }

  while (!input_frames_queue_.empty() && !bitstream_buffers_.empty()) {
    auto& next_input = input_frames_queue_.front();
    if (next_input.resolving_shared_image ||
        (!next_input.frame->HasMappableSharedImage() &&
         next_input.frame->HasSharedImage() &&
         !next_input.resolved_picture.resource)) {
      // D3D12 VEA encodes frames one-by-one, so we will not try following
      // frames.
      break;
    }

    const bool success = DoEncodeTask(next_input, bitstream_buffers_.front());
    input_frames_queue_.pop_front();
    bitstream_buffers_.pop();
    if (!success) {
      break;
    }
  }

  if (flush_requested_ && input_frames_queue_.empty()) {
    flush_requested_ = false;
    child_task_runner_->PostTask(
        FROM_HERE, BindOnce(&D3D12VideoEncodeAccelerator::NotifyFlushDone,
                            child_weak_this_, /*succeed=*/true));
  }
}

bool D3D12VideoEncodeAccelerator::DoEncodeTask(
    InputFrameRef& input_frame,
    const BitstreamBuffer& bitstream_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  scoped_refptr<VideoFrame> frame = input_frame.frame;
  D3D12PictureBuffer picture_buffer;
  // The region of |picture_buffer| that holds the picture content. For textures
  // handed to the encoder untouched, that is the frame's own visible rect. For
  // the shared memory path, CreateResourceForSharedMemoryVideoFrame() already
  // cropped the frame to its visible rect and wrote the result over the whole
  // of the upload texture, so the content covers |config_.input_visible_size|
  // starting at the origin and the frame's visible rect no longer applies.
  gfx::Rect input_visible_rect;
  if (frame->HasMappableSharedImage()) {
    if (frame->HasNativeMappableSharedImage()) {
      picture_buffer = CreateResourceForDXGIHandleBackedVideoFrame(*frame);
      input_visible_rect = frame->visible_rect();
    } else {
      frame = ConvertToMemoryMappedFrame(std::move(frame));
      if (!frame) {
        NotifyError(
            {EncoderStatus::Codes::kInvalidInputFrame,
             "Failed to convert shared memory mappable SI for encoding"});
        return false;
      }
      picture_buffer = CreateResourceForSharedMemoryVideoFrame(*frame);
      input_visible_rect = gfx::Rect(config_.input_visible_size);
    }
  } else if (frame->storage_type() == VideoFrame::STORAGE_SHMEM) {
    picture_buffer = CreateResourceForSharedMemoryVideoFrame(*frame);
    input_visible_rect = gfx::Rect(config_.input_visible_size);
  } else if (frame->HasSharedImage()) {
    picture_buffer = input_frame.resolved_picture;
    input_visible_rect = frame->visible_rect();
  } else {
    NotifyError({EncoderStatus::Codes::kInvalidInputFrame,
                 "Unsupported frame storage type for encoding"});
    return false;
  }
  if (!picture_buffer.resource) {
    NotifyError({EncoderStatus::Codes::kInvalidInputFrame,
                 "Failed to create input_texture"});
    return false;
  }

  auto result_or_error = encoder_->Encode(
      picture_buffer, input_visible_rect, frame->ColorSpace(), bitstream_buffer,
      input_frame.options, frame->hdr_metadata());
  if (!result_or_error.has_value()) {
    NotifyError(std::move(result_or_error).error());
    return false;
  }

  D3D12VideoEncodeDelegate::EncodeResult result =
      std::move(result_or_error).value();
  result.metadata.timestamp = frame->timestamp();

  if (metrics_helper_) {
    metrics_helper_->EncodeOneFrame(
        result.metadata.key_frame,
        base::TimeTicks::Now() - input_frame.frame_encode_start_time);
  }
  if (!encoded_at_least_one_frame_) {
    encoded_at_least_one_frame_ = true;
  }

  child_task_runner_->PostTask(
      FROM_HERE, BindOnce(
                     [](base::WeakPtr<Client> client, int32_t id,
                        const BitstreamBufferMetadata& md,
                        Microsoft::WRL::ComPtr<SharedImageReadLock> access) {
                       if (client) {
                         client->BitstreamBufferReady(id, md);
                       }
                     },
                     client_, result.bitstream_buffer_id, result.metadata,
                     std::move(input_frame.scoped_read_access)));
  return true;
}

void D3D12VideoEncodeAccelerator::DestroyTask() {
  DVLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  delete this;
}

void D3D12VideoEncodeAccelerator::NotifyError(EncoderStatus status) {
  // We return here when `error_occurred_` was already true, as this is not the
  // first error that is reported.
  if (error_occurred_.exchange(true)) {
    return;
  }

  CHECK(!status.is_ok());
  base::UmaHistogramEnumeration(
      GetEncoderStatusHistogramName(config_.output_profile), status.code());

  if (!child_task_runner_->RunsTasksInCurrentSequence()) {
    child_task_runner_->PostTask(
        FROM_HERE,
        BindOnce(&D3D12VideoEncodeAccelerator::NotifyErrorOnChildSequence,
                 child_weak_this_, std::move(status)));
    return;
  }

  NotifyErrorOnChildSequence(std::move(status));
}

void D3D12VideoEncodeAccelerator::NotifyErrorOnChildSequence(
    EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);
  MEDIA_LOG(ERROR, media_log_)
      << "D3D12VEA error " << static_cast<int32_t>(status.code()) << ": "
      << status.message();
  if (client_) {
    client_->NotifyErrorStatus(status);
    client_ptr_factory_->InvalidateWeakPtrs();
  }
}

std::unique_ptr<D3D11To12Fence> Create11On12InteropFence(
    ID3D12Device* d3d12_device,
    ID3D11Device* d3d11_device) {
  CHECK(d3d12_device);
#define RETURN_NULLPTR_ON_HR_FAILURE(hr, message)                          \
  if (FAILED(hr)) {                                                        \
    LOG(ERROR) << message << ": " << logging::SystemErrorCodeToString(hr); \
    return nullptr;                                                        \
  }

  if (!d3d11_device) {
    LOG(ERROR) << "D3D11 device is null.";
    return nullptr;
  }
  Microsoft::WRL::ComPtr<ID3D11Device5> device5;
  auto hr = d3d11_device->QueryInterface(IID_PPV_ARGS(&device5));
  RETURN_NULLPTR_ON_HR_FAILURE(hr, "Failed to query ID3D11Device5");

  Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_fence;
  hr = device5->CreateFence(0, D3D11_FENCE_FLAG_SHARED,
                            IID_PPV_ARGS(&d3d11_fence));
  RETURN_NULLPTR_ON_HR_FAILURE(hr, "Failed to create fence");
  HANDLE handle = nullptr;
  hr = d3d11_fence->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &handle);
  RETURN_NULLPTR_ON_HR_FAILURE(hr, "Failed to create a shared fence handle");
  base::win::ScopedHandle scoped_handle(handle);

  Microsoft::WRL::ComPtr<ID3D12Fence> d3d12_fence;
  hr = d3d12_device->OpenSharedHandle(handle, IID_PPV_ARGS(&d3d12_fence));
  RETURN_NULLPTR_ON_HR_FAILURE(hr, "Failed to open shared fence handle");

  return std::make_unique<D3D11To12Fence>(std::move(d3d11_fence),
                                          std::move(d3d12_fence));

#undef RETURN_NULLPTR_ON_HR_FAILURE
}

void D3D12VideoEncodeAccelerator::OnCommandBufferHelperAvailable(
    GetCommandBufferHelperResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  command_buffer_helper_ = std::move(result.command_buffer_helper);
  source_texture_fence_ = std::move(result.source_texture_fence);

  if (!source_texture_fence_) {
    return NotifyError(
        {EncoderStatus::Codes::kD3D12CreateFenceFailed,
         "Failed to create interop fence for shared image encoding"});
  }
  acquired_command_buffer_ = true;

  // Resolve frames in the queue that are waiting for command buffer
  // availability.
  ResolveQueuedSharedImages();
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
                     get_command_buffer_helper_cb, device_),
      base::BindPostTask(
          encoder_task_runner_,
          base::BindOnce(
              &D3D12VideoEncodeAccelerator::OnCommandBufferHelperAvailable,
              encoder_weak_this_)));
}

// This runs on the encoder task runner. It does not replace the original
// video frame. Instead it will attach a resolved ID3D12Resource to
// corresponding entry in the `input_frames_queue_`.
void D3D12VideoEncodeAccelerator::OnSharedImageResolved(
    scoped_refptr<VideoFrame> frame,
    base::win::ScopedHandle shared_handle,
    Microsoft::WRL::ComPtr<SharedImageReadLock> scoped_read_access,
    uint64_t source_texture_fence_value,
    HRESULT hr) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  if (FAILED(hr)) {
    MEDIA_LOG(ERROR, media_log_)
        << "Failed to resolve shared image for frame, error code: " << std::hex
        << hr;
    return NotifyError({EncoderStatus::Codes::kSharedImageResolveFailed,
                        "Failed to resolve shared image"});
  }
  Microsoft::WRL::ComPtr<ID3D12Resource> input_texture;
  hr = device_->OpenSharedHandle(shared_handle.Get(),
                                 IID_PPV_ARGS(&input_texture));
  if (FAILED(hr)) {
    return NotifyError({EncoderStatus::Codes::kD3D12OpenSharedHandleFailed,
                        "Failed to open shared handle for D3D12 resource"});
  }

  // Find the matching frame in the queue and update it.
  auto it = std::ranges::find_if(input_frames_queue_,
                                 [&](const InputFrameRef& input_frame) {
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
  it->scoped_read_access = std::move(scoped_read_access);
  it->resolved_picture = {
      std::move(input_texture),
      0,
      {source_texture_fence_->GetD3D12Fence(), source_texture_fence_value},
  };

  // Check if we can encode the front frames now.
  TryEncodeFrames();
}

void D3D12VideoEncodeAccelerator::ResolveQueuedSharedImages() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  for (auto& input_frame : input_frames_queue_) {
    if (!input_frame.frame->HasMappableSharedImage() &&
        input_frame.frame->HasSharedImage() &&
        !input_frame.resolve_shared_image_requested) {
      input_frame.resolve_shared_image_requested = true;
      gpu_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &D3D12GenerateResourceFromSharedImageVideoFrame,
              input_frame.frame,
              source_texture_fence_->GetD3D11FenceAndIncrementValue(),
              command_buffer_helper_,
              base::BindPostTask(
                  encoder_task_runner_,
                  base::BindOnce(
                      &D3D12VideoEncodeAccelerator::OnSharedImageResolved,
                      encoder_weak_this_))));
    }
  }
}

bool D3D12VideoEncodeAccelerator::IsFlushSupported() {
  return true;
}

void D3D12VideoEncodeAccelerator::Flush(FlushCallback flush_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);

  if (destroy_requested_) {
    std::move(flush_callback).Run(/*succeed=*/false);
    return;
  }

  flush_callback_ = std::move(flush_callback);
  encoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&D3D12VideoEncodeAccelerator::FlushTask,
                                encoder_weak_this_));
}

void D3D12VideoEncodeAccelerator::FlushTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  if (!encoder_) {
    child_task_runner_->PostTask(
        FROM_HERE, BindOnce(&D3D12VideoEncodeAccelerator::NotifyFlushDone,
                            child_weak_this_, /*succeed=*/false));
    return;
  }

  flush_requested_ = true;
  TryEncodeFrames();
}

void D3D12VideoEncodeAccelerator::NotifyFlushDone(bool succeed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);

  std::move(flush_callback_).Run(succeed);
}

}  // namespace media
