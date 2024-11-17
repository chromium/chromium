// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/mf_video_processor_accelerator.h"

#include <mfapi.h>

#include "media/base/win/mf_helpers.h"

namespace media {

MediaFoundationVideoProcessorAccelerator::
    MediaFoundationVideoProcessorAccelerator(
        const gpu::GpuPreferences& gpu_preferences,
        const gpu::GpuDriverBugWorkarounds& gpu_workarounds)
    : workarounds_(gpu_workarounds) {}

MediaFoundationVideoProcessorAccelerator::
    ~MediaFoundationVideoProcessorAccelerator() {
  DVLOG(3) << __func__;
}

bool MediaFoundationVideoProcessorAccelerator::Initialize(
    const Config& config,
    scoped_refptr<DXGIDeviceManager> dxgi_device_manager,
    std::unique_ptr<MediaLog> media_log) {
  dxgi_device_manager_ = std::move(dxgi_device_manager);
  media_log_ = std::move(media_log);

  return InitializeVideoProcessor(config);
}

bool MediaFoundationVideoProcessorAccelerator::InitializeVideoProcessor(
    const Config& config) {
  HRESULT hr =
      CoCreateInstance(CLSID_VideoProcessorMFT, nullptr, CLSCTX_INPROC_SERVER,
                       IID_PPV_ARGS(&video_processor_));
  RETURN_ON_HR_FAILURE(hr, "Couldn't activate video processor", false);

  if (dxgi_device_manager_) {
    auto mf_dxgi_device_manager =
        dxgi_device_manager_->GetMFDXGIDeviceManager();
    hr = video_processor_->ProcessMessage(
        MFT_MESSAGE_SET_D3D_MANAGER,
        reinterpret_cast<ULONG_PTR>(mf_dxgi_device_manager.Get()));
    RETURN_ON_HR_FAILURE(hr, "Couldn't set D3D manager", false);
  }

  // Allow encoder binding of output samples
  ComMFAttributes output_attributes;
  hr = video_processor_->GetOutputStreamAttributes(0, &output_attributes);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get output attributes", false);
  hr = output_attributes->SetUINT32(MF_SA_D3D11_BINDFLAGS,
                                    D3D11_BIND_VIDEO_ENCODER);
  RETURN_ON_HR_FAILURE(hr, L"Couldn't set encoder bind flags", false);

  // Disable frame rate conversion; this allows the MFT to be 1-in
  // 1-out, avoiding latency and extra complexity.  If frame rate
  // conversion is implemented for this component, the sample
  // processing model will need to change to accommodate as the
  // processor will hold multiple input samples before it
  // generates an output sample.
  ComMFAttributes mft_attributes;
  hr = video_processor_->GetAttributes(&mft_attributes);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get MFT attributes", false);
  hr = mft_attributes->SetUINT32(MF_XVP_DISABLE_FRC, 1);
  RETURN_ON_HR_FAILURE(hr, "Couldn't disable FRC", false);

  hr = MFCreateMediaType(&input_media_type_);
  RETURN_ON_HR_FAILURE(hr, "Couldn't create input media type", false);

  hr = input_media_type_->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set major type", false);
  hr = input_media_type_->SetGUID(
      MF_MT_SUBTYPE, VideoPixelFormatToMFSubtype(config.input_format));
  RETURN_ON_HR_FAILURE(hr, "Couldn't set subtype", false);
  hr = MFSetAttributeSize(input_media_type_.Get(), MF_MT_FRAME_SIZE,
                          config.input_visible_size.width(),
                          config.input_visible_size.height());
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame size", false);
  hr = input_media_type_->SetUINT32(MF_MT_COMPRESSED, 0);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set compressed flag", false);
  hr = input_media_type_->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set independent samples flag", false);
  hr = input_media_type_->SetUINT32(
      MF_MT_SAMPLE_SIZE, VideoFrame::AllocationSize(config.input_format,
                                                    config.input_visible_size));
  RETURN_ON_HR_FAILURE(hr, "Couldn't set sample size", false);
  hr = MFSetAttributeRatio(input_media_type_.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1,
                           1);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set pixel aspect ratio", false);
  hr = input_media_type_->SetUINT32(
      MF_MT_VIDEO_PRIMARIES, VideoPrimariesToMFVideoPrimaries(
                                 config.input_color_space.GetPrimaryID()));
  RETURN_ON_HR_FAILURE(hr, "Couldn't set video primaries", false);
  if (config.input_format == PIXEL_FORMAT_XRGB ||
      config.input_format == PIXEL_FORMAT_ARGB) {
    // The video processor will calculate stride, but for RGB formats
    // it uses a bottom-up stride.  Set the stride explicitly for RGB.
    hr = input_media_type_->SetUINT32(MF_MT_DEFAULT_STRIDE,
                                      config.input_visible_size.width() * 4);
    RETURN_ON_HR_FAILURE(hr, "Couldn't set default stride", false);
  }

  // This component currently is not used for frame rate conversion.  If needed,
  // the actual frame rate should be set here.
  hr = MFSetAttributeRatio(input_media_type_.Get(), MF_MT_FRAME_RATE, 30, 1);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame rate", false);

  // This component currently is not used for deinterlacing.  If needed,
  // the actual interlace mode should be set here.
  hr = input_media_type_->SetUINT32(MF_MT_INTERLACE_MODE,
                                    MFVideoInterlace_Progressive);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set interlace mode", false);

  ComMFMediaType output_media_type;
  hr = MFCreateMediaType(&output_media_type);
  RETURN_ON_HR_FAILURE(hr, "Couldn't create output media type", false);

  hr = output_media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  RETURN_ON_HR_FAILURE(hr, L"Couldn't set major type", false);
  hr = output_media_type->SetGUID(
      MF_MT_SUBTYPE, VideoPixelFormatToMFSubtype(config.output_format));
  RETURN_ON_HR_FAILURE(hr, L"Couldn't set subtype", false);
  hr = MFSetAttributeSize(output_media_type.Get(), MF_MT_FRAME_SIZE,
                          config.output_visible_size.width(),
                          config.output_visible_size.height());
  RETURN_ON_HR_FAILURE(hr, L"Couldn't set frame size", false);
  hr = output_media_type->SetUINT32(MF_MT_COMPRESSED, 0);
  RETURN_ON_HR_FAILURE(hr, L"Couldn't set compressed flag", false);
  hr = output_media_type->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1);
  RETURN_ON_HR_FAILURE(hr, L"Couldn't set independent samples flag", false);
  hr = MFSetAttributeRatio(output_media_type.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1,
                           1);
  RETURN_ON_HR_FAILURE(hr, L"Couldn't set pixel aspect ratio", false);
  hr = output_media_type->SetUINT32(
      MF_MT_VIDEO_PRIMARIES, VideoPrimariesToMFVideoPrimaries(
                                 config.output_color_space.GetPrimaryID()));
  RETURN_ON_HR_FAILURE(hr, L"Couldn't set video primaries", false);
  hr = output_media_type->SetUINT32(MF_MT_INTERLACE_MODE,
                                    MFVideoInterlace_Progressive);
  RETURN_ON_HR_FAILURE(hr, L"Couldn't set interlace mode", false);

  // If doing frame rate conversion, the actual frame rate should be
  // set here.
  hr = MFSetAttributeRatio(output_media_type.Get(), MF_MT_FRAME_RATE, 30, 1);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame rate", false);

  hr = video_processor_->SetInputType(0, input_media_type_.Get(), 0);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set input media type", false);
  hr = video_processor_->SetOutputType(0, output_media_type.Get(), 0);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set output media type", false);

  hr = video_processor_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
  RETURN_ON_HR_FAILURE(hr, "Couldn't notify begin streaming", false);
  hr = video_processor_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
  RETURN_ON_HR_FAILURE(hr, "Coudln't notify start of stream", false);

  return true;
}

HRESULT MediaFoundationVideoProcessorAccelerator::Convert(
    scoped_refptr<VideoFrame> frame,
    IMFSample** sample_out) {
  DCHECK(video_processor_ != nullptr);

  ComMFSample sample;
  HRESULT hr = GenerateSampleFromVideoFrame(
      frame.get(), dxgi_device_manager_.get(), true, nullptr, 0, &sample);
  RETURN_ON_HR_FAILURE(hr, L"Couldn't generate MF sample from VideoFrame", hr);

  hr = AdjustInputSizeIfNeeded(sample.Get());
  RETURN_ON_HR_FAILURE(hr, L"Couldn't adjust input size for new frame", hr);

  // The video processor will internally acquire the keyed mutex for the
  // underlying texture when it is needed.  No need to synchronize here.
  return Convert(sample.Get(), sample_out);
}

HRESULT MediaFoundationVideoProcessorAccelerator::Convert(
    IMFSample* sample,
    IMFSample** sample_out) {
  // This approach of feeding an input to the MFT and expecting an output
  // only works if frame rate conversion is off (MF_XVP_DISABLE_FRC).  If this
  // component needs to do frame rate conversion, this logic will need
  // to be reworked to allow for multiple input samples before an output
  // is generated.
  DCHECK(video_processor_ != nullptr);
  HRESULT hr = video_processor_->ProcessInput(0, sample, 0);
  RETURN_ON_HR_FAILURE(hr, L"Failed ProcessInput for video processing", hr);

  MFT_OUTPUT_STREAM_INFO stream_info;
  hr = video_processor_->GetOutputStreamInfo(0, &stream_info);
  RETURN_ON_HR_FAILURE(hr, L"Couldn't get output stream info from XVP", hr);

  MFT_OUTPUT_DATA_BUFFER data_buffer;
  data_buffer.dwStreamID = 0;
  data_buffer.pSample = nullptr;
  data_buffer.dwStatus = 0;
  data_buffer.pEvents = nullptr;

  ComMFSample sample_for_output;
  if (!(stream_info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) &&
      !(stream_info.dwFlags & MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)) {
    // In D3D mode, the XVP should always provide samples.  This block
    // handles software processing mode.
    ComMFMediaBuffer media_buffer;

    hr = MFCreateMemoryBuffer(stream_info.cbSize, &media_buffer);
    RETURN_ON_HR_FAILURE(hr, L"Couldn't create output memory buffer", hr);
    hr = MFCreateSample(&sample_for_output);
    data_buffer.pSample = sample_for_output.Get();
    RETURN_ON_HR_FAILURE(hr, L"Couldn't create output sample", hr);
    hr = data_buffer.pSample->AddBuffer(media_buffer.Get());
    RETURN_ON_HR_FAILURE(hr, L"Couldn't add output buffer", hr);
  }

  DWORD status;
  hr = video_processor_->ProcessOutput(0, 1, &data_buffer, &status);
  RETURN_ON_HR_FAILURE(hr, L"Failed ProcessOutput for video processing", hr);

  if (data_buffer.pEvents) {
    data_buffer.pEvents->Release();
  }

  *sample_out = data_buffer.pSample;
  if (sample_for_output) {
    sample_for_output.Detach();
  }

  return S_OK;
}

HRESULT MediaFoundationVideoProcessorAccelerator::AdjustInputSizeIfNeeded(
    IMFSample* sample) {
  ComMFMediaBuffer buffer;
  HRESULT hr = sample->GetBufferByIndex(0, &buffer);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get sample buffer", hr);
  Microsoft::WRL::ComPtr<IMFDXGIBuffer> dxgi_buffer;
  hr = buffer.As(&dxgi_buffer);
  if (SUCCEEDED(hr)) {
    ComD3D11Texture2D texture;
    hr = dxgi_buffer->GetResource(IID_PPV_ARGS(&texture));
    RETURN_ON_HR_FAILURE(hr, "Couldn't get texture from DXGI buffer", hr);
    D3D11_TEXTURE2D_DESC input_desc = {};
    texture->GetDesc(&input_desc);

    UINT32 mt_width = input_desc.Width;
    UINT32 mt_height = input_desc.Height;
    (void)MFGetAttributeSize(input_media_type_.Get(), MF_MT_FRAME_SIZE,
                             &mt_width, &mt_height);
    if (input_desc.Width != mt_width || input_desc.Height != mt_height) {
      ComMFMediaType new_input_media_type;
      hr = MFCreateMediaType(&new_input_media_type);
      RETURN_ON_HR_FAILURE(hr, "Couldn't create new input media type", hr);
      hr = input_media_type_->CopyAllItems(new_input_media_type.Get());
      RETURN_ON_HR_FAILURE(hr, "Couldn't clone input media type", hr);
      hr = MFSetAttributeSize(new_input_media_type.Get(), MF_MT_FRAME_SIZE,
                              input_desc.Width, input_desc.Height);
      RETURN_ON_HR_FAILURE(hr, "Couldn't set new frame size", hr);
      hr = video_processor_->SetInputType(0, new_input_media_type.Get(), 0);
      RETURN_ON_HR_FAILURE(hr, "Couldn't set new input type on video processor",
                           hr);
      input_media_type_ = new_input_media_type;
    }
  }

  return S_OK;
}

}  // namespace media
