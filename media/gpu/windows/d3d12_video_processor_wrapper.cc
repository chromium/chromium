// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_video_processor_wrapper.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "media/gpu/windows/d3d12_helpers.h"
#include "ui/gfx/color_space_win.h"

namespace media {

D3D12VideoProcessorWrapper::D3D12VideoProcessorWrapper(
    ComD3D12VideoDevice video_device)
    : video_device_(video_device) {
  CHECK_EQ(video_device.As(&device_), S_OK);
}

D3D12VideoProcessorWrapper::~D3D12VideoProcessorWrapper() = default;

bool D3D12VideoProcessorWrapper::Init() {
  D3D12_COMMAND_QUEUE_DESC command_queue_desc{
      D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS};
  HRESULT hr = device_->CreateCommandQueue(&command_queue_desc,
                                           IID_PPV_ARGS(&command_queue_));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Create video process command queue failed: "
                << logging::SystemErrorCodeToString(hr);
    return false;
  }

  hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS,
                                       IID_PPV_ARGS(&command_allocator_));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Create video process command allocator failed: "
                << logging::SystemErrorCodeToString(hr);
    return false;
  }

  hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS,
                                  command_allocator_.Get(), nullptr,
                                  IID_PPV_ARGS(&command_list_));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Create video process command list failed: "
                << logging::SystemErrorCodeToString(hr);
    return false;
  }

  hr = command_list_->Close();
  if (FAILED(hr)) {
    DLOG(ERROR) << "Close video process command list failed: "
                << logging::SystemErrorCodeToString(hr);
    return false;
  }

  fence_ = D3D12Fence::Create(device_.Get());
  if (!fence_) {
    DLOG(ERROR) << "Failed to create D3D12Fence";
    return false;
  }

  return true;
}

bool D3D12VideoProcessorWrapper::ProcessFrames(
    ID3D12Resource* input_texture,
    UINT input_subresource,
    const gfx::ColorSpace& input_color_space,
    const gfx::Rect& input_rectangle,
    ID3D12Resource* output_texture,
    UINT output_subresource,
    const gfx::ColorSpace& output_color_space,
    const gfx::Rect& output_rectangle) {
  DCHECK(command_queue_ && command_allocator_ && command_list_ && fence_);
  D3D12_RESOURCE_DESC input_texture_desc = input_texture->GetDesc();
  D3D12_RESOURCE_DESC output_texture_desc = output_texture->GetDesc();
  D3D12_VIDEO_SIZE_RANGE source_size_range{
      static_cast<UINT>(input_texture_desc.Width), input_texture_desc.Height,
      static_cast<UINT>(input_texture_desc.Width), input_texture_desc.Height};
  D3D12_VIDEO_SIZE_RANGE destination_size_range{
      static_cast<UINT>(output_texture_desc.Width), output_texture_desc.Height,
      static_cast<UINT>(output_texture_desc.Width), output_texture_desc.Height};
  D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC input_stream_desc{
      .Format = input_texture_desc.Format,
      .ColorSpace = gfx::ColorSpaceWin::GetDXGIColorSpace(input_color_space),
      .FrameRate = {30, 1},
      .SourceSizeRange = source_size_range,
      .DestinationSizeRange = destination_size_range,
  };
  D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC output_stream_desc{
      .Format = output_texture_desc.Format,
      .ColorSpace = gfx::ColorSpaceWin::GetDXGIColorSpace(output_color_space),
      .FrameRate = {30, 1},
  };

  HRESULT hr;
  if (memcmp(&input_stream_desc, &input_stream_desc_,
             sizeof(D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC)) != 0 ||
      memcmp(&output_stream_desc, &output_stream_desc_,
             sizeof(D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC)) != 0) {
    D3D12_FEATURE_DATA_VIDEO_PROCESS_SUPPORT support{
        .InputSample = {.Width = static_cast<UINT>(input_texture_desc.Width),
                        .Height = input_texture_desc.Height,
                        .Format = {.Format = input_texture_desc.Format,
                                   .ColorSpace = input_stream_desc.ColorSpace}},
        .InputFrameRate = {30, 1},
        .OutputFormat = {.Format = output_texture_desc.Format,
                         .ColorSpace = output_stream_desc.ColorSpace},
        .OutputFrameRate = {30, 1},
    };
    hr = video_device_->CheckFeatureSupport(D3D12_FEATURE_VIDEO_PROCESS_SUPPORT,
                                            &support, sizeof(support));
    if (FAILED(hr)) {
      DLOG(ERROR) << "CheckFeatureSupport for "
                     "D3D12_FEATURE_VIDEO_PROCESS_SUPPORT failed: "
                  << logging::SystemErrorCodeToString(hr);
    }
    if (support.SupportFlags != D3D12_VIDEO_PROCESS_SUPPORT_FLAG_SUPPORTED) {
      DLOG(ERROR) << "D3D12 cannot support video processing.";
      return false;
    }

    hr = video_device_->CreateVideoProcessor(0, &output_stream_desc, 1,
                                             &input_stream_desc,
                                             IID_PPV_ARGS(&video_processor_));
    if (FAILED(hr)) {
      DLOG(ERROR) << "Create video processor failed: "
                  << logging::SystemErrorCodeToString(hr);
      return false;
    }
    input_stream_desc_ = input_stream_desc;
    output_stream_desc_ = output_stream_desc;
  }

  hr = command_allocator_->Reset();
  if (FAILED(hr)) {
    DLOG(ERROR) << "Reset video process command allocator failed:"
                << logging::SystemErrorCodeToString(hr);
    return false;
  }

  hr = command_list_->Reset(command_allocator_.Get());
  if (FAILED(hr)) {
    DLOG(ERROR) << "Reset video process command list failed:"
                << logging::SystemErrorCodeToString(hr);
    return false;
  }

  std::vector<D3D12_RESOURCE_BARRIER> resource_barriers;
  auto barriers1 = CreateD3D12TransitionBarriersForAllPlanes(
      input_texture, input_subresource, D3D12_RESOURCE_STATE_COMMON,
      D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ);
  auto barriers2 = CreateD3D12TransitionBarriersForAllPlanes(
      output_texture, output_subresource, D3D12_RESOURCE_STATE_COMMON,
      D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE);
  resource_barriers.insert(resource_barriers.end(), barriers1.begin(),
                           barriers1.end());
  resource_barriers.insert(resource_barriers.end(), barriers2.begin(),
                           barriers2.end());
  command_list_->ResourceBarrier(resource_barriers.size(),
                                 resource_barriers.data());

  D3D12_VIDEO_PROCESS_INPUT_STREAM_ARGUMENTS input_stream_arguments{
      .InputStream = {{.pTexture2D = input_texture,
                       .Subresource = input_subresource}},
      .Transform = {.SourceRectangle = input_rectangle.ToRECT(),
                    .DestinationRectangle = output_rectangle.ToRECT()}};
  D3D12_VIDEO_PROCESS_OUTPUT_STREAM_ARGUMENTS output_stream_arguments{
      .OutputStream = {
          {.pTexture2D = output_texture, .Subresource = output_subresource}}};
  command_list_->ProcessFrames(video_processor_.Get(), &output_stream_arguments,
                               1, &input_stream_arguments);

  for (auto& barrier : resource_barriers) {
    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
  }
  command_list_->ResourceBarrier(resource_barriers.size(),
                                 resource_barriers.data());

  hr = command_list_->Close();
  if (FAILED(hr)) {
    DLOG(ERROR) << "Close video process command list failed:"
                << logging::SystemErrorCodeToString(hr);
    return false;
  }

  ID3D12CommandList* command_list = command_list_.Get();
  command_queue_->ExecuteCommandLists(1, &command_list);

  return fence_->SignalAndWait(*command_queue_.Get()) == D3D11StatusCode::kOk;
}

}  // namespace media
