// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_DELEGATE_H_
#define MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_DELEGATE_H_

#include "third_party/microsoft_dxheaders/src/include/directx/d3d12.h"
// Windows SDK headers should be included after DirectX headers.

#include <wrl.h>

#include "base/functional/callback.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/encoder_status.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d12_video_encoder_wrapper.h"
#include "media/gpu/windows/d3d12_video_helpers.h"
#include "media/gpu/windows/d3d12_video_processor_wrapper.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace media {

class MEDIA_GPU_EXPORT D3D12VideoEncodeDelegate {
 public:
  static constexpr size_t kAV1DPBMaxSize = 8;
  struct EncodeResult {
    int32_t bitstream_buffer_id_;
    BitstreamBufferMetadata metadata_;
  };

  // Returns the supported profiles for all available codecs.
  static VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles(
      ID3D12VideoDevice3* video_device);

  explicit D3D12VideoEncodeDelegate(
      Microsoft::WRL::ComPtr<ID3D12VideoDevice3> video_device);
  virtual ~D3D12VideoEncodeDelegate();

  virtual EncoderStatus Initialize(VideoEncodeAccelerator::Config config);
  virtual size_t GetMaxNumOfRefFrames() const = 0;
  // Returns whether the delegate supports changing |Bitrate::Mode| using
  // |UpdateRateControl()| during encoding.
  virtual bool SupportsRateControlReconfiguration() const = 0;
  virtual bool ReportsAverageQp() const;

  virtual bool UpdateRateControl(const Bitrate& bitrate, uint32_t framerate);

  // Do video processing if the input frame format or resolution is not
  // expected and then call |EncodeImpl()|.
  virtual EncoderStatus::Or<EncodeResult> Encode(
      Microsoft::WRL::ComPtr<ID3D12Resource> input_frame,
      UINT input_frame_subresource,
      const gfx::ColorSpace& input_frame_color_space,
      const BitstreamBuffer& bitstream_buffer,
      const VideoEncoder::EncodeOptions& options);

  // Do the codec specific encoding.
  virtual EncoderStatus::Or<BitstreamBufferMetadata> EncodeImpl(
      ID3D12Resource* input_frame,
      UINT input_frame_subresource,
      const VideoEncoder::EncodeOptions& options) = 0;

  void SetFactoriesForTesting(
      base::RepeatingCallback<decltype(CreateD3D12VideoEncoderWrapper)>
          video_encoder_wrapper_factory,
      base::RepeatingCallback<
          decltype(std::make_unique<D3D12VideoProcessorWrapper,
                                    Microsoft::WRL::ComPtr<ID3D12VideoDevice>>)>
          video_processor_wrapper_factory) {
    video_encoder_wrapper_factory_ = std::move(video_encoder_wrapper_factory);
    video_processor_wrapper_factory_ =
        std::move(video_processor_wrapper_factory);
  }

  D3D12VideoEncoderWrapper* GetVideoEncoderWrapperForTesting() {
    return video_encoder_wrapper_.get();
  }

  D3D12VideoProcessorWrapper* GetVideoProcessorWrapperForTesting() {
    return video_processor_wrapper_.get();
  }

  DXGI_FORMAT GetFormatForTesting() const { return input_format_; }

 protected:
  class D3D12VideoEncoderRateControl {
   public:
    enum class FrameType { kIntra, kInterPrev, kInterBiDirectional };

    // Creates an uninitialized rate control.
    D3D12VideoEncoderRateControl();

    D3D12VideoEncoderRateControl(const D3D12VideoEncoderRateControl& other);
    D3D12VideoEncoderRateControl& operator=(
        const D3D12VideoEncoderRateControl& other);

    static D3D12VideoEncoderRateControl CreateCqp(uint32_t i_frame_qp,
                                                  uint32_t p_frame_qp,
                                                  uint32_t b_frame_qp);
    static D3D12VideoEncoderRateControl Create(Bitrate bitrate,
                                               uint32_t framerate);

    D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE GetMode() const;

    void SetCQP(FrameType frame_type, uint32_t qp);

    const D3D12_VIDEO_ENCODER_RATE_CONTROL& GetD3D12VideoEncoderRateControl()
        const {
      return rate_control_;
    }

    bool operator==(const D3D12VideoEncoderRateControl& other) const;

   private:
    D3D12_VIDEO_ENCODER_RATE_CONTROL rate_control_{};
    // We will assign the union member later, so the default initialization is
    // fine.
    union {
      D3D12_VIDEO_ENCODER_RATE_CONTROL_CQP cqp;
      D3D12_VIDEO_ENCODER_RATE_CONTROL_CBR cbr;
      D3D12_VIDEO_ENCODER_RATE_CONTROL_VBR vbr;
    } params_{};
  };

  virtual EncoderStatus InitializeVideoEncoder(
      const VideoEncodeAccelerator::Config& config) = 0;

  virtual EncoderStatus::Or<size_t> ReadbackBitstream(
      base::span<uint8_t> bitstream_buffer);

  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  Microsoft::WRL::ComPtr<ID3D12VideoDevice3> video_device_;
  uint32_t max_num_ref_frames_ = 0;

  // The the size and format for the input of the D3D12VideoEncoder. The format
  // may be different to input frame, in which case we do internal conversion.
  D3D12_VIDEO_ENCODER_PICTURE_RESOLUTION_DESC input_size_{};
  DXGI_FORMAT input_format_ = DXGI_FORMAT_UNKNOWN;

  D3D12VideoEncoderRateControl rate_control_;

  // Output profile requested by |config| in |Initialize()|.
  // The implementation may use a different profile for compatibility.
  VideoCodecProfile output_profile_ = VIDEO_CODEC_PROFILE_UNKNOWN;

  // The video encoder factory that may be changed for testing.
  base::RepeatingCallback<decltype(CreateD3D12VideoEncoderWrapper)>
      video_encoder_wrapper_factory_ =
          base::BindRepeating(&CreateD3D12VideoEncoderWrapper);
  std::unique_ptr<D3D12VideoEncoderWrapper> video_encoder_wrapper_;

 private:
  // The video processor factory that may be changed for testing.
  base::RepeatingCallback<
      decltype(std::make_unique<D3D12VideoProcessorWrapper,
                                Microsoft::WRL::ComPtr<ID3D12VideoDevice>>)>
      video_processor_wrapper_factory_ = base::BindRepeating(
          &std::make_unique<D3D12VideoProcessorWrapper,
                            Microsoft::WRL::ComPtr<ID3D12VideoDevice>>);
  // The video processor used for possible resolution, format, or color space
  // conversion.
  std::unique_ptr<D3D12VideoProcessorWrapper> video_processor_wrapper_;
  Microsoft::WRL::ComPtr<ID3D12Resource> processed_input_frame_;
};

// Records an ID3D12Resource pointer and a subresource index for a specific
// subresource.
struct D3D12PictureBuffer {
  raw_ptr<ID3D12Resource> resource_ = nullptr;
  UINT subresource_ = 0;
};

// A class to manage the decoded picture buffers for D3D12 video encode and
// returns the |D3D12_VIDEO_ENCODE_REFERENCE_FRAMES| that D3D12 video encode API
// expects. When initialized, it creates the textures that may be needed during
// the encoding. Before encoding a frame, the client should call
// |GetCurrentFrame()| to get an unused buffer. After encoding a frame, depends
// on the codec, call |InsertCurrentFrame()| or |ReplaceWithCurrentFrame()| to
// update the decoded picture buffers.
template <size_t maxDpbSize>
class D3D12VideoEncodeDecodedPictureBuffers {
 public:
  D3D12VideoEncodeDecodedPictureBuffers();
  virtual ~D3D12VideoEncodeDecodedPictureBuffers();

  size_t size() const { return size_; }

  // Initialize the texture array with the given size and format.
  bool InitializeTextureArray(ID3D12Device* device,
                              gfx::Size texture_size,
                              DXGI_FORMAT format,
                              size_t max_num_ref_frames);

  // Get the unused buffer for current frame.
  D3D12PictureBuffer GetCurrentFrame() const;
  // Insert the last picture buffer returned by |GetCurrentFrame()| into the
  // given index and move the old buffers with index no less than |position| to
  // the next index.
  void InsertCurrentFrame(size_t position);
  // Replace the picture buffer at |position| with the last picture buffer
  // returned by |GetCurrentFrame()|.
  void ReplaceWithCurrentFrame(size_t position);

  // Return the |D3D12_VIDEO_ENCODE_REFERENCE_FRAMES| structure that D3D12 video
  // encode API expects.
  virtual D3D12_VIDEO_ENCODE_REFERENCE_FRAMES
  ToD3D12VideoEncodeReferenceFrames();

 private:
  size_t size_ = 0;
  absl::InlinedVector<Microsoft::WRL::ComPtr<ID3D12Resource>, maxDpbSize + 1>
      resources_;
  absl::InlinedVector<ID3D12Resource*, maxDpbSize + 1> raw_resources_;
  absl::InlinedVector<UINT, maxDpbSize + 1> subresources_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_DELEGATE_H_
