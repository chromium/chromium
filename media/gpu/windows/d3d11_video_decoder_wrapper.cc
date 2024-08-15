// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/windows/d3d11_video_decoder_wrapper.h"

#include <d3d9.h>

#include "base/check_op.h"
#include "base/strings/string_number_conversions.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace media {

namespace {

D3D11_VIDEO_DECODER_BUFFER_TYPE
BufferTypeToD3D11BufferType(D3DVideoDecoderWrapper::BufferType type) {
  switch (type) {
    case D3DVideoDecoderWrapper::BufferType::kPictureParameters:
      return D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS;
    case D3DVideoDecoderWrapper::BufferType::kInverseQuantizationMatrix:
      return D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX;
    case D3DVideoDecoderWrapper::BufferType::kSliceControl:
      return D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL;
    case D3DVideoDecoderWrapper::BufferType::kBitstream:
      return D3D11_VIDEO_DECODER_BUFFER_BITSTREAM;
  }
  NOTREACHED();
}

template <typename D3D11VideoContext, typename D3D11VideoDecoderBufferDesc>
class ScopedD3D11DecoderBuffer;

template <typename D3D11VideoContext, typename D3D11VideoDecoderBufferDesc>
class D3D11VideoDecoderWrapperImpl : public D3D11VideoDecoderWrapper {
 public:
  D3D11VideoDecoderWrapperImpl(
      MediaLog* media_log,
      ComD3D11VideoDevice video_device,
      Microsoft::WRL::ComPtr<D3D11VideoContext> video_context,
      ComD3D11VideoDecoder video_decoder)
      : D3D11VideoDecoderWrapper(media_log),
        video_device_(std::move(video_device)),
        video_context_(std::move(video_context)),
        video_decoder_(std::move(video_decoder)) {}
  ~D3D11VideoDecoderWrapperImpl() override {
    // Release the scoped buffer before video_context_ and video_decoder_
    // destructs.
    if (bitstream_buffer_) {
      bitstream_buffer_.reset();
    }
  }

  std::optional<bool> UseSingleTexture() const override {
    D3D11_VIDEO_DECODER_DESC desc;
    D3D11_VIDEO_DECODER_CONFIG config;
    HRESULT hr = video_decoder_->GetCreationParameters(&desc, &config);
    if (FAILED(hr)) {
      MEDIA_PLOG(ERROR, hr, media_log_)
          << "D3D11VideoDecoder GetCreationParameters failed";
      return std::nullopt;
    }
    // Prefer whatever the config tells us about whether to use one Texture2D
    // with multiple array slices, or multiple Texture2Ds with one slice each.
    // If bit 14 is clear, then it's the former, else it's the latter.
    //
    // Let the workaround override array texture mode, if enabled.
    // TODO(crbug.com/41463736): Ignore |use_single_video_decoder_texture_|
    // here, since it might be the case that it's not actually the right fix.
    // Instead, We use this workaround to force a copy later.  The workaround
    // will be renamed if this turns out to fix the issue, but we might need to
    // merge back and smaller changes are better.
    //
    // For more information, please see:
    // https://download.microsoft.com/download/9/2/A/92A4E198-67E0-4ABD-9DB7-635D711C2752/DXVA_VPx.pdf
    // https://download.microsoft.com/download/5/f/c/5fc4ec5c-bd8c-4624-8034-319c1bab7671/DXVA_H264.pdf
    // TODO(crbug.com/dawn/1932): Use array textures if preferred with shared
    // handles once Dawn supports importing those.
    return config.ConfigDecoderSpecific & (1 << 14);
  }

  void Reset() override {
    if (bitstream_buffer_) {
      CHECK(bitstream_buffer_->Commit());
      bitstream_buffer_.reset();
    }
  }

  bool WaitForFrameBegins(D3D11PictureBuffer* output_picture) override {
    auto result = output_picture->AcquireOutputView();
    if (!result.has_value()) {
      media_log_->NotifyError(std::move(result).error().AddHere());
      return false;
    }
    ID3D11VideoDecoderOutputView* output_view = std::move(result).value();
    HRESULT hr;
    do {
      hr = video_context_->DecoderBeginFrame(video_decoder_.Get(), output_view,
                                             0, nullptr);
      if (hr == E_PENDING || hr == D3DERR_WASSTILLDRAWING) {
        // Hardware is busy.  We should make the call again.
        base::PlatformThread::YieldCurrentThread();
      }
    } while (hr == E_PENDING || hr == D3DERR_WASSTILLDRAWING);

    if (FAILED(hr)) {
      MEDIA_PLOG(ERROR, hr, media_log_) << "DecoderBeginFrame failed";
      return false;
    }

    return true;
  }

  bool HasPendingBuffer(BufferType type) override {
    D3D11_VIDEO_DECODER_BUFFER_TYPE d3d_buffer_type =
        BufferTypeToD3D11BufferType(type);
    for (auto video_buffer : video_buffers_) {
      if (video_buffer.BufferType == d3d_buffer_type) {
        return true;
      }
    }

    return false;
  }

  bool SubmitSlice() override {
    if (!slice_info_bytes_.empty()) {
      auto buffer = GetSliceControlBuffer(slice_info_bytes_.size());
      if (buffer.size() < slice_info_bytes_.size()) {
        MEDIA_LOG(ERROR, media_log_) << "Insufficient slice info buffer size";
        return false;
      }

      memcpy(buffer.data(), slice_info_bytes_.data(), slice_info_bytes_.size());
      slice_info_bytes_.clear();

      if (!buffer.Commit()) {
        return false;
      }
    }

    return SubmitBitstreamBuffer() && SubmitDecoderBuffers();
  }

  bool SubmitDecode() override {
    HRESULT hr = video_context_->DecoderEndFrame(video_decoder_.Get());
    if (FAILED(hr)) {
      MEDIA_PLOG(ERROR, hr, media_log_) << "SubmitDecode failed";
      return false;
    }
    return true;
  }

 private:
  friend class ScopedD3D11DecoderBuffer<D3D11VideoContext,
                                        D3D11VideoDecoderBufferDesc>;

  std::unique_ptr<ScopedD3DBuffer> GetBuffer(BufferType type,
                                             uint32_t desired_size) override {
    return std::make_unique<ScopedD3D11DecoderBuffer<
        D3D11VideoContext, D3D11VideoDecoderBufferDesc>>(
        this, BufferTypeToD3D11BufferType(type), desired_size, media_log_);
  }

  bool SubmitBitstreamBuffer() {
    DCHECK(bitstream_buffer_);
    bool ok = bitstream_buffer_->Commit();
    bitstream_buffer_.reset();
    return ok;
  }

  bool SubmitDecoderBuffers();

  ComD3D11VideoDevice video_device_;
  Microsoft::WRL::ComPtr<D3D11VideoContext> video_context_;
  ComD3D11VideoDecoder video_decoder_;
  absl::InlinedVector<D3D11VideoDecoderBufferDesc, 4> video_buffers_;
};

template <>
bool D3D11VideoDecoderWrapperImpl<
    ID3D11VideoContext,
    D3D11_VIDEO_DECODER_BUFFER_DESC>::SubmitDecoderBuffers() {
  DCHECK_LE(video_buffers_.size(), 4ull);
  HRESULT hr = video_context_->SubmitDecoderBuffers(
      video_decoder_.Get(), video_buffers_.size(), video_buffers_.data());
  video_buffers_.clear();
  if (FAILED(hr)) {
    MEDIA_PLOG(ERROR, hr, media_log_) << "SubmitDecoderBuffers failed";
    return false;
  }

  return true;
}

template <>
bool D3D11VideoDecoderWrapperImpl<
    ID3D11VideoContext1,
    D3D11_VIDEO_DECODER_BUFFER_DESC1>::SubmitDecoderBuffers() {
  DCHECK_LE(video_buffers_.size(), 4ull);
  HRESULT hr = video_context_->SubmitDecoderBuffers1(
      video_decoder_.Get(), video_buffers_.size(), video_buffers_.data());
  video_buffers_.clear();
  if (FAILED(hr)) {
    MEDIA_PLOG(ERROR, hr, media_log_) << "SubmitDecoderBuffers failed";
    return false;
  }

  return true;
}

template <typename D3D11VideoContext, typename D3D11VideoDecoderBufferDesc>
class ScopedD3D11DecoderBuffer : public ScopedD3DBuffer {
 public:
  ScopedD3D11DecoderBuffer(
      D3D11VideoDecoderWrapperImpl<D3D11VideoContext,
                                   D3D11VideoDecoderBufferDesc>* decoder,
      D3D11_VIDEO_DECODER_BUFFER_TYPE type,
      uint32_t desired_size,
      MediaLog* media_log)
      : decoder_(decoder),
        type_(type),
        desired_size_(desired_size),
        media_log_(media_log->Clone()) {
    UINT size;
    uint8_t* buffer;
    HRESULT hr = decoder_->video_context_->GetDecoderBuffer(
        decoder_->video_decoder_.Get(), type_, &size,
        reinterpret_cast<void**>(&buffer));
    if (FAILED(hr)) {
      D3D11StatusCode status_code = D3D11StatusCode::kOk;
      switch (type_) {
        case D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS:
          status_code = D3D11StatusCode::kGetPicParamBufferFailed;
          break;
        case D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX:
          status_code = D3D11StatusCode::kGetQuantBufferFailed;
          break;
        case D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL:
          status_code = D3D11StatusCode::kGetSliceControlBufferFailed;
          break;
        case D3D11_VIDEO_DECODER_BUFFER_BITSTREAM:
          status_code = D3D11StatusCode::kGetBitstreamBufferFailed;
          break;
        default:
          NOTREACHED();
      }
      media_log_->NotifyError(
          D3D11Status{status_code, "D3D11 GetDecoderBuffer failed", hr});
      return;
    }

    data_ = base::span<uint8_t>(buffer, size);
  }

  ~ScopedD3D11DecoderBuffer() override { Commit(); }

  ScopedD3D11DecoderBuffer(const ScopedD3D11DecoderBuffer&) = delete;
  ScopedD3D11DecoderBuffer& operator=(const ScopedD3D11DecoderBuffer&) = delete;

  bool Commit() override {
    return Commit(std::min(static_cast<size_t>(desired_size_), data_.size()));
  }

  bool Commit(uint32_t written_size) override {
    if (data_.empty()) {
      return false;
    }

    HRESULT hr = decoder_->video_context_->ReleaseDecoderBuffer(
        decoder_->video_decoder_.Get(), type_);
    if (FAILED(hr)) {
      D3D11StatusCode status_code = D3D11StatusCode::kOk;
      switch (type_) {
        case D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS:
          status_code = D3D11StatusCode::kReleasePicParamBufferFailed;
          break;
        case D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX:
          status_code = D3D11StatusCode::kReleaseQuantBufferFailed;
          break;
        case D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL:
          status_code = D3D11StatusCode::kReleaseSliceControlBufferFailed;
          break;
        case D3D11_VIDEO_DECODER_BUFFER_BITSTREAM:
          status_code = D3D11StatusCode::kReleaseBitstreamBufferFailed;
          break;
        default:
          NOTREACHED();
      }
      media_log_->NotifyError(
          D3D11Status{status_code, "D3D11 ReleaseDecoderBuffer failed", hr});
      return false;
    }

    decoder_->video_buffers_.emplace_back(D3D11VideoDecoderBufferDesc{
        .BufferType = type_,
        .DataOffset = 0,
        .DataSize = written_size,
    });
    data_ = base::span<uint8_t>();
    return true;
  }

 private:
  const raw_ptr<D3D11VideoDecoderWrapperImpl<D3D11VideoContext,
                                             D3D11VideoDecoderBufferDesc>>
      decoder_;
  const D3D11_VIDEO_DECODER_BUFFER_TYPE type_;
  const uint32_t desired_size_;
  const std::unique_ptr<MediaLog> media_log_;
};

}  // namespace

// static
std::unique_ptr<D3D11VideoDecoderWrapper> D3D11VideoDecoderWrapper::Create(
    MediaLog* media_log,
    ComD3D11VideoDevice video_device,
    ComD3D11VideoContext video_context,
    const D3D11DecoderConfigurator* decoder_configurator,
    D3D_FEATURE_LEVEL supported_d3d11_version,
    VideoDecoderConfig config) {
  UINT config_count = 0;
  HRESULT hr = video_device->GetVideoDecoderConfigCount(
      decoder_configurator->DecoderDescriptor(), &config_count);
  if (FAILED(hr) || config_count == 0) {
    MEDIA_PLOG(ERROR, hr, media_log) << "GetVideoDecoderConfigCount failed";
    return nullptr;
  }

  D3D11_VIDEO_DECODER_CONFIG dec_config{};
  bool found = false;
  for (UINT i = 0; i < config_count; i++) {
    hr = video_device->GetVideoDecoderConfig(
        decoder_configurator->DecoderDescriptor(), i, &dec_config);
    if (FAILED(hr)) {
      MEDIA_PLOG(ERROR, hr, media_log) << "GetVideoDecoderConfig failed";
      return nullptr;
    }

    // DXVA HEVC, VP9, and AV1 specifications say ConfigBitstreamRaw
    // "shall be 1".
    if (dec_config.ConfigBitstreamRaw == 1 &&
        (config.codec() == VideoCodec::kVP9 ||
         config.codec() == VideoCodec::kAV1 ||
         config.codec() == VideoCodec::kHEVC)) {
      found = true;
      break;
    }

    // ConfigBitstreamRaw == 2 means the decoder uses DXVA_Slice_H264_Short.
    if (dec_config.ConfigBitstreamRaw == 2 &&
        config.codec() == VideoCodec::kH264) {
      found = true;
      break;
    }
  }

  if (!found) {
    MEDIA_LOG(ERROR, media_log) << "VideoDecoderConfig is unsupported";
    return nullptr;
  }

  ComD3D11VideoDecoder video_decoder;
  hr = video_device->CreateVideoDecoder(
      decoder_configurator->DecoderDescriptor(), &dec_config, &video_decoder);
  if (FAILED(hr)) {
    MEDIA_PLOG(ERROR, hr, media_log) << "CreateVideoDecoder failed";
    return nullptr;
  }

  // If we got an 11.1 D3D11 Device, we can use a |ID3D11VideoContext1|,
  // otherwise we have to make sure we only use a |ID3D11VideoContext|.
  if (supported_d3d11_version == D3D_FEATURE_LEVEL_11_0) {
    return std::make_unique<D3D11VideoDecoderWrapperImpl<
        ID3D11VideoContext, D3D11_VIDEO_DECODER_BUFFER_DESC>>(
        media_log, std::move(video_device), std::move(video_context),
        std::move(video_decoder));
  }

  if (supported_d3d11_version == D3D_FEATURE_LEVEL_11_1) {
    ComD3D11VideoContext1 video_context1;
    hr = video_context.As(&video_context1);
    CHECK_EQ(hr, S_OK);
    return std::make_unique<D3D11VideoDecoderWrapperImpl<
        ID3D11VideoContext1, D3D11_VIDEO_DECODER_BUFFER_DESC1>>(
        media_log, std::move(video_device), std::move(video_context1),
        std::move(video_decoder));
  }

  return nullptr;
}

D3D11VideoDecoderWrapper::~D3D11VideoDecoderWrapper() = default;

D3D11VideoDecoderWrapper::D3D11VideoDecoderWrapper(MediaLog* media_log)
    : D3DVideoDecoderWrapper(media_log) {}

}  // namespace media
