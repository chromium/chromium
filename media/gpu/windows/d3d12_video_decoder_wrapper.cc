// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/windows/d3d12_video_decoder_wrapper.h"

#include <Windows.h>

#include <dxva.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_handle.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"
#include "media/gpu/windows/d3d12_fence.h"
#include "media/gpu/windows/d3d12_helpers.h"
#include "media/gpu/windows/d3d12_video_decode_task.h"
#include "media/gpu/windows/scoped_d3d_buffers.h"
#include "media/gpu/windows/supported_profile_helpers.h"

#define RETURN_IF_FAILED(message, status_code, hr)                            \
  do {                                                                        \
    if (FAILED(hr)) {                                                         \
      media_log_->NotifyError<D3D11StatusTraits>({status_code, message, hr}); \
      return false;                                                           \
    }                                                                         \
  } while (0)

#define STATIC_RETURN_IF_FAILED(hr, message)       \
  do {                                             \
    if (FAILED(hr)) {                              \
      MEDIA_PLOG(ERROR, hr, media_log) << message; \
      return nullptr;                              \
    }                                              \
  } while (0)

namespace media {

namespace {

class ScopedD3D12MemoryBuffer;
class ScopedD3D12ResourceBuffer;

class D3D12VideoDecoderWrapperImpl : public D3D12VideoDecoderWrapper {
 public:
  D3D12VideoDecoderWrapperImpl(MediaLog* media_log,
                               ComD3D12Device device,
                               ComD3D12CommandQueue command_queue,
                               ComD3D12VideoDevice video_device,
                               ComD3D12VideoDecoder video_decoder,
                               ComD3D12VideoDecoderHeap video_decoder_heap,
                               scoped_refptr<D3D12Fence> fence,
                               int max_decode_requests)
      : D3D12VideoDecoderWrapper(media_log),
        device_(std::move(device)),
        video_device_(std::move(video_device)),
        command_queue_(std::move(command_queue)),
        video_decoder_(std::move(video_decoder)),
        video_decoder_heap_(std::move(video_decoder_heap)),
        reference_frame_list_(std::move(video_decoder_heap)),
        fence_(std::move(fence)),
        tasks_(max_decode_requests) {
    input_stream_arguments_.pHeap = video_decoder_heap_.Get();
  }

  ~D3D12VideoDecoderWrapperImpl() override = default;

  std::optional<bool> UseSingleTexture() const override { return true; }

  void Reset() override {
    input_stream_arguments_.NumFrameArguments = 0;
    bitstream_buffer_.reset();
  }

  D3D11Status SetPictureBuffers(
      base::span<scoped_refptr<D3D11PictureBuffer>> picture_buffers) override {
    reference_frame_list_.SetPictureBuffers(picture_buffers);
    for (size_t i = 0; i < picture_buffers.size(); ++i) {
      auto result = picture_buffers[i]->ToD3D12Resource(device_.Get());
      if (!result.has_value()) {
        return std::move(result).error();
      }
      reference_frame_list_.emplace(i, std::move(result).value(),
                                    picture_buffers[i]->array_slice());
    }
    return D3D11StatusCode::kOk;
  }

  bool WaitForFrameBegins(D3D11PictureBuffer* output_picture) override {
    TRACE_EVENT("gpu", "D3D12VideoDecoderWrapperImpl::WaitForFrameBegins");
    Reset();

    auto result = output_picture->AcquireOutputView();
    if (!result.has_value()) {
      media_log_->NotifyError<D3D11StatusTraits>(std::move(result).error());
      return false;
    }

    ComD3D11Texture2D d3d11_texture2d = output_picture->Texture();
    D3D11_TEXTURE2D_DESC desc;
    d3d11_texture2d->GetDesc(&desc);
    CHECK(desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE)
        << "Cannot create shared handle for d3d11 texture2d without "
           "D3D11_RESOURCE_MISC_SHARED_NTHANDLE flag.";

    auto d3d12_resource_or_error =
        output_picture->ToD3D12Resource(device_.Get());
    if (!d3d12_resource_or_error.has_value()) {
      return false;
    }
    output_stream_arguments_ = {
        .pOutputTexture2D = std::move(d3d12_resource_or_error).value(),
        .OutputSubresource = output_picture->array_slice(),
    };

    // Reuse the oldest task in the pool.
    current_task_index_ = (current_task_index_ + 1) % tasks_.size();
    D3D12VideoDecoderTask* task = &tasks_[current_task_index_];
    // Wait for the old task to complete, if it has not completed yet, before we
    // can reuse the buffers. In this way we have at most |max_decode_requests|
    // ongoing decoding tasks.
    if (bool ok = task->WaitForCompletion(); !ok) {
      media_log_->NotifyError<D3D11StatusTraits>(
          {D3D11StatusCode::kDecoderBeginFrameFailed,
           "WaitForCompletion failed"});
      return false;
    }
    task->ResetBuffers();

    // The output picture will be ready after completing the fence's value + 1
    // which will be signaled after the decode command list is executed.
    output_picture->SetFenceAndValue(fence_, fence_->Value() + 1);

    return true;
  }

  bool HasPendingBuffer(BufferType type) override {
    D3D12VideoDecoderTask& task = tasks_[current_task_index_];
    switch (type) {
      case BufferType::kPictureParameters:
        return !task.GetPictureParametersBuffer().empty();
      case BufferType::kInverseQuantizationMatrix:
        return !task.GetInverseQuantizationMatrixBuffer().empty();
      case BufferType::kSliceControl:
        return !task.GetSliceControlBuffer().empty();
      case BufferType::kBitstream:
        return bitstream_buffer_.has_value();
    }
    NOTREACHED();
  }

  bool SubmitSlice() override {
    TRACE_EVENT("gpu", "D3D12VideoDecoderWrapperImpl::SubmitSlice");
    D3D12VideoDecoderTask& task = tasks_[current_task_index_];
    if (!slice_info_bytes_.empty()) {
      // In D3D12 we need to submit the frame at once, so SubmitSlice() is
      // expected to be called only once per frame.
      CHECK(task.GetSliceControlBuffer().empty());
      slice_info_bytes_.swap(task.GetSliceControlBuffer());
      slice_info_bytes_.clear();
      CHECK_LT(input_stream_arguments_.NumFrameArguments,
               std::size(input_stream_arguments_.FrameArguments));
      input_stream_arguments_
          .FrameArguments[input_stream_arguments_.NumFrameArguments++] = {
          .Type = D3D12_VIDEO_DECODE_ARGUMENT_TYPE_SLICE_CONTROL,
          .Size = static_cast<UINT>(task.GetSliceControlBuffer().size()),
          .pData = task.GetSliceControlBuffer().data(),
      };
    }
    reference_frame_list_.WriteTo(&input_stream_arguments_.ReferenceFrames);
    CHECK(bitstream_buffer_);
    bitstream_buffer_.reset();
    CHECK_LE(input_stream_arguments_.NumFrameArguments, 4u);
    return true;
  }

  bool SubmitDecode() override {
    TRACE_EVENT("gpu", "D3D12VideoDecoderWrapperImpl::SubmitDecode");
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> command_allocator =
        tasks_[current_task_index_].ResetAndGetCommandAllocator(device_.Get());
    if (!command_allocator) {
      media_log_->NotifyError<D3D11StatusTraits>(
          {D3D11StatusCode::kDecoderBeginFrameFailed,
           "ResetAndGetCommandAllocator failed"});
      return false;
    }
    HRESULT hr;
    if (!command_list_) {
      TRACE_EVENT("gpu", "CreateCommandList");
      hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE,
                                      command_allocator.Get(), nullptr,
                                      IID_PPV_ARGS(&command_list_));
      RETURN_IF_FAILED("Failed to create command list",
                       D3D11StatusCode::kDecoderBeginFrameFailed, hr);
    } else {
      TRACE_EVENT("gpu", "ResetCommandList");
      hr = command_list_->Reset(command_allocator.Get());
    }
    RETURN_IF_FAILED("Failed to reset command list",
                     D3D11StatusCode::kDecoderBeginFrameFailed, hr);

    auto barriers = reference_frame_list_.GetTransitionsToDecodeState(
        output_stream_arguments_.pOutputTexture2D,
        output_stream_arguments_.OutputSubresource);
    command_list_->ResourceBarrier(barriers.size(), barriers.data());

    command_list_->DecodeFrame(video_decoder_.Get(), &output_stream_arguments_,
                               &input_stream_arguments_);

    for (D3D12_RESOURCE_BARRIER& barrier : barriers) {
      std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    }
    command_list_->ResourceBarrier(barriers.size(), barriers.data());

    hr = command_list_->Close();
    RETURN_IF_FAILED("Failed to close command list",
                     D3D11Status::Codes::kSubmitDecoderBuffersFailed, hr);

    ID3D12CommandList* command_lists[] = {command_list_.Get()};
    command_queue_->ExecuteCommandLists(std::size(command_lists),
                                        command_lists);

    auto fence_value_or_error = fence_->Signal(*command_queue_.Get());
    if (!fence_value_or_error.has_value()) {
      media_log_->NotifyError<D3D11StatusTraits>(
          std::move(fence_value_or_error).error());
      return false;
    }
    uint64_t fence_value = std::move(fence_value_or_error).value();
    // Let the task know when it can be reused.
    tasks_[current_task_index_].SetFenceAndValue(fence_, fence_value);
    return true;
  }

 private:
  friend class ScopedD3D12MemoryBuffer;
  friend class ScopedD3D12ResourceBuffer;

  std::unique_ptr<ScopedD3DBuffer> GetBuffer(BufferType type,
                                             uint32_t desired_size) override;

  ComD3D12Device device_;
  ComD3D12VideoDevice video_device_;
  ComD3D12CommandQueue command_queue_;
  ComD3D12VideoDecodeCommandList command_list_;

  ComD3D12VideoDecoder video_decoder_;
  ComD3D12VideoDecoderHeap video_decoder_heap_;
  D3D12_VIDEO_DECODE_INPUT_STREAM_ARGUMENTS input_stream_arguments_{};
  D3D12_VIDEO_DECODE_OUTPUT_STREAM_ARGUMENTS output_stream_arguments_{};
  D3D12ReferenceFrameList reference_frame_list_;

  scoped_refptr<D3D12Fence> fence_;
  // The task pool for multiple ongoing decode requests.
  std::vector<D3D12VideoDecoderTask> tasks_;
  uint32_t current_task_index_ = 0;
};

class ScopedD3D12MemoryBuffer : public ScopedD3DBuffer {
 public:
  ScopedD3D12MemoryBuffer(D3D12VideoDecoderWrapperImpl* decoder,
                          D3D12_VIDEO_DECODE_ARGUMENT_TYPE type,
                          base::span<uint8_t> data)
      : ScopedD3DBuffer(data), decoder_(*decoder), type_(type) {}
  ~ScopedD3D12MemoryBuffer() override { ScopedD3D12MemoryBuffer::Commit(); }

  ScopedD3D12MemoryBuffer(ScopedD3D12MemoryBuffer&) = delete;
  ScopedD3D12MemoryBuffer& operator=(ScopedD3D12MemoryBuffer&) = delete;

  bool Commit() override { return Commit(data_.size()); }

  bool Commit(uint32_t written_size) override {
    if (data_.empty()) {
      return false;
    }
    CHECK_LE(written_size, data_.size());
    CHECK_LT(decoder_->input_stream_arguments_.NumFrameArguments,
             std::size(decoder_->input_stream_arguments_.FrameArguments));
    decoder_->input_stream_arguments_
        .FrameArguments[decoder_->input_stream_arguments_.NumFrameArguments++] =
        {
            .Type = type_,
            .Size = written_size,
            .pData = data_.data(),
        };
    data_ = base::span<uint8_t>();
    return true;
  }

 private:
  const raw_ref<D3D12VideoDecoderWrapperImpl> decoder_;
  const D3D12_VIDEO_DECODE_ARGUMENT_TYPE type_;
};

class ScopedD3D12ResourceBuffer : public ScopedD3DBuffer {
 public:
  ScopedD3D12ResourceBuffer(D3D12VideoDecoderWrapperImpl* decoder,
                            ComD3D12Resource resource,
                            MediaLog* media_log)
      : resource_(std::move(resource)), media_log_(media_log->Clone()) {
    void* mapped_data;
    HRESULT hr = resource_->Map(0, nullptr, &mapped_data);
    if (FAILED(hr)) {
      MEDIA_PLOG(ERROR, hr, media_log_)
          << "Failed to map data of ID3D12Resource";
      return;
    }
    data_ = base::span(reinterpret_cast<uint8_t*>(mapped_data),
                       static_cast<size_t>(resource_->GetDesc().Width));
  }
  ~ScopedD3D12ResourceBuffer() override { ScopedD3D12ResourceBuffer::Commit(); }

  ScopedD3D12ResourceBuffer(ScopedD3D12ResourceBuffer&) = delete;
  ScopedD3D12ResourceBuffer& operator=(ScopedD3D12ResourceBuffer&) = delete;

  bool Commit() override { return Commit(resource_->GetDesc().Width); }

  bool Commit(uint32_t written_size) override {
    if (data_.empty()) {
      return false;
    }
    CHECK_LE(written_size, data_.size());
    D3D12_RANGE range{0, written_size};
    resource_->Unmap(0, &range);
    data_ = base::span<uint8_t>();
    return true;
  }

 protected:
  ComD3D12Resource resource_;
  const std::unique_ptr<MediaLog> media_log_;
};

std::unique_ptr<ScopedD3DBuffer> D3D12VideoDecoderWrapperImpl::GetBuffer(
    BufferType type,
    uint32_t desired_size) {
  TRACE_EVENT("gpu", "D3D12VideoDecoderWrapperImpl::GetBuffer");
  D3D12VideoDecoderTask& task = tasks_[current_task_index_];
  switch (type) {
    case BufferType::kPictureParameters:
      if (task.GetPictureParametersBuffer().size() < desired_size) {
        task.GetPictureParametersBuffer().resize(desired_size);
      }
      return std::make_unique<ScopedD3D12MemoryBuffer>(
          this, D3D12_VIDEO_DECODE_ARGUMENT_TYPE_PICTURE_PARAMETERS,
          task.GetPictureParametersBuffer());
    case BufferType::kInverseQuantizationMatrix:
      if (task.GetInverseQuantizationMatrixBuffer().size() < desired_size) {
        task.GetInverseQuantizationMatrixBuffer().resize(desired_size);
      }
      return std::make_unique<ScopedD3D12MemoryBuffer>(
          this, D3D12_VIDEO_DECODE_ARGUMENT_TYPE_INVERSE_QUANTIZATION_MATRIX,
          task.GetInverseQuantizationMatrixBuffer());
    case BufferType::kSliceControl:
      if (task.GetSliceControlBuffer().size() < desired_size) {
        task.GetSliceControlBuffer().resize(desired_size);
      }
      return std::make_unique<ScopedD3D12MemoryBuffer>(
          this, D3D12_VIDEO_DECODE_ARGUMENT_TYPE_SLICE_CONTROL,
          task.GetSliceControlBuffer());
    case BufferType::kBitstream:
      Microsoft::WRL::ComPtr<ID3D12Resource> bitstream_buffer =
          task.GetBitstreamBuffer(device_.Get(), desired_size);
      if (!bitstream_buffer) {
        return std::make_unique<ScopedD3D12MemoryBuffer>(
            this, D3D12_VIDEO_DECODE_ARGUMENT_TYPE{}, base::span<uint8_t>{});
      }
      input_stream_arguments_.CompressedBitstream = {
          .pBuffer = bitstream_buffer.Get(),
          // The size of a buffer resource is its width.
          .Size = bitstream_buffer->GetDesc().Width,
      };
      return std::make_unique<ScopedD3D12ResourceBuffer>(this, bitstream_buffer,
                                                         media_log_);
  }
  NOTREACHED();
}

}  // namespace

// static
std::unique_ptr<D3D12VideoDecoderWrapper> D3D12VideoDecoderWrapper::Create(
    MediaLog* media_log,
    ComD3D12VideoDevice video_device,
    VideoDecoderConfig config,
    uint8_t bit_depth,
    VideoChromaSampling chroma_sampling,
    int max_decode_requests) {
  GUID guid =
      GetD3D12VideoDecodeGUID(config.profile(), bit_depth, chroma_sampling);
  DXGI_FORMAT decode_format = GetOutputDXGIFormat(bit_depth, chroma_sampling);
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  if (guid == DXVA_ModeHEVC_VLD_Main12) {
    constexpr UINT kNVIDIADeviceId = 0x10DE;
    ComDXGIDevice dxgi_device;
    if (SUCCEEDED(video_device.As(&dxgi_device)) &&
        GetGPUVendorID(dxgi_device) == kNVIDIADeviceId) {
      // NVIDIA driver requires output format to be P010 for HEVC 12b420 range
      // extension profile.
      decode_format = DXGI_FORMAT_P010;
    }
  }
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

  if (guid == DXVA_ModeAV1_VLD_Profile1) {
    // AV1 profile 1 is YUV 4:4:4 only.
    if (bit_depth == 8) {
      decode_format = DXGI_FORMAT_AYUV;
    } else if (bit_depth == 10) {
      decode_format = DXGI_FORMAT_Y410;
    }
  }

  D3D12_FEATURE_DATA_VIDEO_DECODE_SUPPORT feature{
      .Configuration = {guid},
      .Width = static_cast<UINT>(config.coded_size().width()),
      .Height = static_cast<UINT>(config.coded_size().height()),
      .DecodeFormat = decode_format,
  };
  HRESULT hr = video_device->CheckFeatureSupport(
      D3D12_FEATURE_VIDEO_DECODE_SUPPORT, &feature, sizeof(feature));
  STATIC_RETURN_IF_FAILED(hr, "D3D12VideoDevice CheckFeatureSupport failed");
  if (feature.SupportFlags != D3D12_VIDEO_DECODE_SUPPORT_FLAG_SUPPORTED) {
    MEDIA_LOG(ERROR, media_log)
        << "D3D12VideoDecoder does not support profile "
        << GetProfileName(config.profile()) << " with bit depth "
        << base::strict_cast<int>(bit_depth) << ", chroma subsampling format "
        << VideoChromaSamplingToString(chroma_sampling) << ", coded size "
        << config.coded_size().ToString() << ", and output dxgi format "
        << decode_format;
    return nullptr;
  }

  ComD3D12VideoDecoder video_decoder;
  D3D12_VIDEO_DECODER_DESC desc{.Configuration = {guid}};
  hr = video_device->CreateVideoDecoder(&desc, IID_PPV_ARGS(&video_decoder));
  STATIC_RETURN_IF_FAILED(hr, "D3D12VideoDevice CreateVideoDecoder failed");

  D3D12_VIDEO_DECODER_HEAP_DESC heap_desc{
      .Configuration = {guid},
      .DecodeWidth = static_cast<UINT>(config.coded_size().width()),
      .DecodeHeight = static_cast<UINT>(config.coded_size().height()),
      .Format = decode_format,
  };
  ComD3D12VideoDecoderHeap video_decoder_heap;
  hr = video_device->CreateVideoDecoderHeap(&heap_desc,
                                            IID_PPV_ARGS(&video_decoder_heap));
  STATIC_RETURN_IF_FAILED(hr, "D3D12VideoDevice CreateVideoDecoderHeap failed");

  ComD3D12Device device;
  hr = video_device.As(&device);
  CHECK_EQ(hr, S_OK);

  // TODO(crbug.com/40233230): Share the command queue across video decoders.
  ComD3D12CommandQueue command_queue;
  D3D12_COMMAND_QUEUE_DESC command_queue_desc{
      D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE};
  hr = device->CreateCommandQueue(&command_queue_desc,
                                  IID_PPV_ARGS(&command_queue));
  STATIC_RETURN_IF_FAILED(hr, "D3D12Device CreateCommandQueue failed");

  scoped_refptr<D3D12Fence> fence =
      D3D12Fence::Create(device.Get(), D3D12_FENCE_FLAG_SHARED);
  if (!fence) {
    return nullptr;
  }

  return std::make_unique<D3D12VideoDecoderWrapperImpl>(
      media_log, std::move(device), std::move(command_queue),
      std::move(video_device), std::move(video_decoder),
      std::move(video_decoder_heap), std::move(fence), max_decode_requests);
}

D3D12VideoDecoderWrapper::D3D12VideoDecoderWrapper(MediaLog* media_log)
    : D3DVideoDecoderWrapper(media_log) {}

}  // namespace media
