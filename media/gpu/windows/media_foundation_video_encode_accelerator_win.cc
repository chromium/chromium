// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/media_foundation_video_encode_accelerator_win.h"

#pragma warning(push)
#pragma warning(disable : 4800)  // Disable warning for added padding.

#include <codecapi.h>
#include <mferror.h>
#include <mftransform.h>
#include <objbase.h>

#include <iterator>
#include <utility>
#include <vector>

#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "media/base/win/mf_helpers.h"
#include "media/base/win/mf_initializer.h"
#include "third_party/libyuv/include/libyuv.h"

using media::mf::MediaBufferScopedPointer;

namespace media {

namespace {

const int32_t kDefaultTargetBitrate = 5000000;
const size_t kMaxFrameRateNumerator = 30;
const size_t kMaxFrameRateDenominator = 1;
const size_t kMaxResolutionWidth = 1920;
const size_t kMaxResolutionHeight = 1088;
const size_t kNumInputBuffers = 3;
// Media Foundation uses 100 nanosecond units for time, see
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms697282(v=vs.85).aspx
const size_t kOneMicrosecondInMFSampleTimeUnits = 10;
const size_t kOutputSampleBufferSizeRatio = 4;

constexpr const wchar_t* const kMediaFoundationVideoEncoderDLLs[] = {
    L"mf.dll", L"mfplat.dll",
};

// Resolutions that some platforms support, should be listed in ascending order.
constexpr const gfx::Size kOptionalMaxResolutions[] = {gfx::Size(3840, 2176)};

eAVEncH264VProfile GetH264VProfile(VideoCodecProfile profile) {
  switch (profile) {
    case H264PROFILE_BASELINE:
      return eAVEncH264VProfile_Base;
    case H264PROFILE_MAIN:
      return eAVEncH264VProfile_Main;
    case H264PROFILE_HIGH: {
      // eAVEncH264VProfile_High requires Windows 8.
      if (base::win::GetVersion() < base::win::Version::WIN8) {
        return eAVEncH264VProfile_unknown;
      }
      return eAVEncH264VProfile_High;
    }
    default:
      return eAVEncH264VProfile_unknown;
  }
}

}  // namespace

class MediaFoundationVideoEncodeAccelerator::EncodeOutput {
 public:
  EncodeOutput(uint32_t size, bool key_frame, base::TimeDelta timestamp)
      : keyframe(key_frame), capture_timestamp(timestamp), data_(size) {}

  uint8_t* memory() { return data_.data(); }

  int size() const { return static_cast<int>(data_.size()); }

  const bool keyframe;
  const base::TimeDelta capture_timestamp;

 private:
  std::vector<uint8_t> data_;

  DISALLOW_COPY_AND_ASSIGN(EncodeOutput);
};

struct MediaFoundationVideoEncodeAccelerator::BitstreamBufferRef {
  BitstreamBufferRef(int32_t id,
                     base::WritableSharedMemoryMapping mapping,
                     size_t size)
      : id(id), mapping(std::move(mapping)), size(size) {}
  const int32_t id;
  const base::WritableSharedMemoryMapping mapping;
  const size_t size;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BitstreamBufferRef);
};

// TODO(zijiehe): Respect |compatible_with_win7_| in the implementation. Some
// attributes are not supported by Windows 7, setting them will return errors.
// See bug: http://crbug.com/777659.
MediaFoundationVideoEncodeAccelerator::MediaFoundationVideoEncodeAccelerator(
    bool compatible_with_win7)
    : compatible_with_win7_(compatible_with_win7),
      main_client_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      encoder_thread_("MFEncoderThread") {}

MediaFoundationVideoEncodeAccelerator::
    ~MediaFoundationVideoEncodeAccelerator() {
  DVLOG(3) << __func__;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  DCHECK(!encoder_thread_.IsRunning());
  DCHECK(!encoder_task_weak_factory_.HasWeakPtrs());
}

VideoEncodeAccelerator::SupportedProfiles
MediaFoundationVideoEncodeAccelerator::GetSupportedProfiles() {
  TRACE_EVENT0("gpu,startup",
               "MediaFoundationVideoEncodeAccelerator::GetSupportedProfiles");
  DVLOG(3) << __func__;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  SupportedProfiles profiles;
  target_bitrate_ = kDefaultTargetBitrate;
  frame_rate_ = kMaxFrameRateNumerator / kMaxFrameRateDenominator;
  input_visible_size_ = gfx::Size(kMaxResolutionWidth, kMaxResolutionHeight);
  if (!CreateHardwareEncoderMFT() || !SetEncoderModes() ||
      !InitializeInputOutputSamples(H264PROFILE_BASELINE)) {
    ReleaseEncoderResources();
    DVLOG(1)
        << "Hardware encode acceleration is not available on this platform.";
    return profiles;
  }

  gfx::Size highest_supported_resolution = input_visible_size_;
  for (const auto& resolution : kOptionalMaxResolutions) {
    DCHECK_GT(resolution.GetArea(), highest_supported_resolution.GetArea());
    if (!IsResolutionSupported(resolution))
      break;
    highest_supported_resolution = resolution;
  }
  ReleaseEncoderResources();

  SupportedProfile profile;
  // More profiles can be supported here, but they should be available in SW
  // fallback as well.
  profile.profile = H264PROFILE_BASELINE;
  profile.max_framerate_numerator = kMaxFrameRateNumerator;
  profile.max_framerate_denominator = kMaxFrameRateDenominator;
  profile.max_resolution = highest_supported_resolution;
  profiles.push_back(profile);

  profile.profile = H264PROFILE_MAIN;
  profiles.push_back(profile);

  profile.profile = H264PROFILE_HIGH;
  profiles.push_back(profile);

  return profiles;
}

bool MediaFoundationVideoEncodeAccelerator::Initialize(const Config& config,
                                                       Client* client) {
  DVLOG(3) << __func__ << ": " << config.AsHumanReadableString();
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  if (PIXEL_FORMAT_I420 != config.input_format) {
    DLOG(ERROR) << "Input format not supported= "
                << VideoPixelFormatToString(config.input_format);
    return false;
  }

  if (GetH264VProfile(config.output_profile) == eAVEncH264VProfile_unknown) {
    DLOG(ERROR) << "Output profile not supported= " << config.output_profile;
    return false;
  }

  encoder_thread_.init_com_with_mta(false);
  if (!encoder_thread_.Start()) {
    DLOG(ERROR) << "Failed spawning encoder thread.";
    return false;
  }
  encoder_thread_task_runner_ = encoder_thread_.task_runner();

  if (!CreateHardwareEncoderMFT()) {
    DLOG(ERROR) << "Failed creating a hardware encoder MFT.";
    return false;
  }

  main_client_weak_factory_.reset(new base::WeakPtrFactory<Client>(client));
  main_client_ = main_client_weak_factory_->GetWeakPtr();
  input_visible_size_ = config.input_visible_size;
  frame_rate_ = kMaxFrameRateNumerator / kMaxFrameRateDenominator;
  target_bitrate_ = config.initial_bitrate;
  bitstream_buffer_size_ = config.input_visible_size.GetArea();
  u_plane_offset_ =
      VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::kYPlane,
                            input_visible_size_)
          .GetArea();
  v_plane_offset_ = u_plane_offset_ + VideoFrame::PlaneSize(PIXEL_FORMAT_I420,
                                                            VideoFrame::kUPlane,
                                                            input_visible_size_)
                                          .GetArea();
  y_stride_ = VideoFrame::RowBytes(VideoFrame::kYPlane, PIXEL_FORMAT_I420,
                                   input_visible_size_.width());
  u_stride_ = VideoFrame::RowBytes(VideoFrame::kUPlane, PIXEL_FORMAT_I420,
                                   input_visible_size_.width());
  v_stride_ = VideoFrame::RowBytes(VideoFrame::kVPlane, PIXEL_FORMAT_I420,
                                   input_visible_size_.width());

  if (!SetEncoderModes()) {
    DLOG(ERROR) << "Failed setting encoder parameters.";
    return false;
  }

  if (!InitializeInputOutputSamples(config.output_profile)) {
    DLOG(ERROR) << "Failed initializing input-output samples.";
    return false;
  }

  MFT_INPUT_STREAM_INFO input_stream_info;
  HRESULT hr =
      encoder_->GetInputStreamInfo(input_stream_id_, &input_stream_info);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get input stream info", false);
  input_sample_ = mf::CreateEmptySampleWithBuffer(
      input_stream_info.cbSize
          ? input_stream_info.cbSize
          : VideoFrame::AllocationSize(PIXEL_FORMAT_I420, input_visible_size_),
      input_stream_info.cbAlignment);

  MFT_OUTPUT_STREAM_INFO output_stream_info;
  hr = encoder_->GetOutputStreamInfo(output_stream_id_, &output_stream_info);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get output stream info", false);
  output_sample_ = mf::CreateEmptySampleWithBuffer(
      output_stream_info.cbSize
          ? output_stream_info.cbSize
          : bitstream_buffer_size_ * kOutputSampleBufferSizeRatio,
      output_stream_info.cbAlignment);

  hr = encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set ProcessMessage", false);

  main_client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::RequireBitstreamBuffers, main_client_,
                                kNumInputBuffers, input_visible_size_,
                                bitstream_buffer_size_));
  return SUCCEEDED(hr);
}

void MediaFoundationVideoEncodeAccelerator::Encode(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe) {
  DVLOG(3) << __func__;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaFoundationVideoEncodeAccelerator::EncodeTask,
                     encoder_task_weak_factory_.GetWeakPtr(), std::move(frame),
                     force_keyframe));
}

void MediaFoundationVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    BitstreamBuffer buffer) {
  DVLOG(3) << __func__ << ": buffer size=" << buffer.size();
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  if (buffer.size() < bitstream_buffer_size_) {
    DLOG(ERROR) << "Output BitstreamBuffer isn't big enough: " << buffer.size()
                << " vs. " << bitstream_buffer_size_;
    NotifyError(kInvalidArgumentError);
    return;
  }

  auto region =
      base::UnsafeSharedMemoryRegion::Deserialize(buffer.TakeRegion());
  auto mapping = region.Map();
  if (!region.IsValid() || !mapping.IsValid()) {
    DLOG(ERROR) << "Failed mapping shared memory.";
    NotifyError(kPlatformFailureError);
    return;
  }
  // After mapping, |region| is no longer necessary and it can be
  // destroyed. |mapping| will keep the shared memory region open.

  std::unique_ptr<BitstreamBufferRef> buffer_ref(
      new BitstreamBufferRef(buffer.id(), std::move(mapping), buffer.size()));
  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MediaFoundationVideoEncodeAccelerator::UseOutputBitstreamBufferTask,
          encoder_task_weak_factory_.GetWeakPtr(), base::Passed(&buffer_ref)));
}

void MediaFoundationVideoEncodeAccelerator::RequestEncodingParametersChange(
    uint32_t bitrate,
    uint32_t framerate) {
  DVLOG(3) << __func__ << ": bitrate=" << bitrate
           << ": framerate=" << framerate;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  encoder_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MediaFoundationVideoEncodeAccelerator::
                                    RequestEncodingParametersChangeTask,
                                encoder_task_weak_factory_.GetWeakPtr(),
                                bitrate, framerate));
}

void MediaFoundationVideoEncodeAccelerator::Destroy() {
  DVLOG(3) << __func__;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  // Cancel all callbacks.
  main_client_weak_factory_.reset();

  if (encoder_thread_.IsRunning()) {
    encoder_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaFoundationVideoEncodeAccelerator::DestroyTask,
                       encoder_task_weak_factory_.GetWeakPtr()));
    encoder_thread_.Stop();
  }

  delete this;
}

// static
bool MediaFoundationVideoEncodeAccelerator::PreSandboxInitialization() {
  bool result = true;
  for (const wchar_t* mfdll : kMediaFoundationVideoEncoderDLLs) {
    if (::LoadLibrary(mfdll) == nullptr) {
      result = false;
    }
  }
  return result;
}

bool MediaFoundationVideoEncodeAccelerator::CreateHardwareEncoderMFT() {
  DVLOG(3) << __func__;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  if (!compatible_with_win7_ &&
      base::win::GetVersion() < base::win::Version::WIN8) {
    DVLOG(ERROR) << "Windows versions earlier than 8 are not supported.";
    return false;
  }

  for (const wchar_t* mfdll : kMediaFoundationVideoEncoderDLLs) {
    if (!::GetModuleHandle(mfdll)) {
      DVLOG(ERROR) << mfdll << " is required for encoding";
      return false;
    }
  }

  if (!InitializeMediaFoundation())
    return false;

  uint32_t flags = MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER;
  MFT_REGISTER_TYPE_INFO input_info;
  input_info.guidMajorType = MFMediaType_Video;
  input_info.guidSubtype = MFVideoFormat_NV12;
  MFT_REGISTER_TYPE_INFO output_info;
  output_info.guidMajorType = MFMediaType_Video;
  output_info.guidSubtype = MFVideoFormat_H264;

  base::win::ScopedCoMem<CLSID> CLSIDs;
  uint32_t count = 0;
  HRESULT hr = MFTEnum(MFT_CATEGORY_VIDEO_ENCODER, flags, &input_info,
                       &output_info, NULL, &CLSIDs, &count);
  RETURN_ON_HR_FAILURE(hr, "Couldn't enumerate hardware encoder", false);
  RETURN_ON_FAILURE((count > 0), "No HW encoder found", false);
  DVLOG(3) << "HW encoder(s) found: " << count;
  hr = ::CoCreateInstance(CLSIDs[0], nullptr, CLSCTX_ALL,
                          IID_PPV_ARGS(&encoder_));
  RETURN_ON_HR_FAILURE(hr, "Couldn't activate hardware encoder", false);
  RETURN_ON_FAILURE((encoder_.Get() != nullptr),
                    "No HW encoder instance created", false);
  return true;
}

bool MediaFoundationVideoEncodeAccelerator::InitializeInputOutputSamples(
    VideoCodecProfile output_profile) {
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  DWORD input_count = 0;
  DWORD output_count = 0;
  HRESULT hr = encoder_->GetStreamCount(&input_count, &output_count);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get stream count", false);
  if (input_count < 1 || output_count < 1) {
    LOG(ERROR) << "Stream count too few: input " << input_count << ", output "
               << output_count;
    return false;
  }

  std::vector<DWORD> input_ids(input_count, 0);
  std::vector<DWORD> output_ids(output_count, 0);
  hr = encoder_->GetStreamIDs(input_count, input_ids.data(), output_count,
                              output_ids.data());
  if (hr == S_OK) {
    input_stream_id_ = input_ids[0];
    output_stream_id_ = output_ids[0];
  } else if (hr == E_NOTIMPL) {
    input_stream_id_ = 0;
    output_stream_id_ = 0;
  } else {
    LOG(ERROR) << "Couldn't find stream ids.";
    return false;
  }

  // Initialize output parameters.
  hr = MFCreateMediaType(imf_output_media_type_.GetAddressOf());
  RETURN_ON_HR_FAILURE(hr, "Couldn't create media type", false);
  hr = imf_output_media_type_->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set media type", false);
  hr = imf_output_media_type_->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set video format", false);
  hr = imf_output_media_type_->SetUINT32(MF_MT_AVG_BITRATE, target_bitrate_);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set bitrate", false);
  hr = MFSetAttributeRatio(imf_output_media_type_.Get(), MF_MT_FRAME_RATE,
                           frame_rate_, 1);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame rate", false);
  hr = MFSetAttributeSize(imf_output_media_type_.Get(), MF_MT_FRAME_SIZE,
                          input_visible_size_.width(),
                          input_visible_size_.height());
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame size", false);
  hr = imf_output_media_type_->SetUINT32(MF_MT_INTERLACE_MODE,
                                         MFVideoInterlace_Progressive);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set interlace mode", false);
  hr = imf_output_media_type_->SetUINT32(MF_MT_MPEG2_PROFILE,
                                         GetH264VProfile(output_profile));
  RETURN_ON_HR_FAILURE(hr, "Couldn't set codec profile", false);
  hr = encoder_->SetOutputType(output_stream_id_, imf_output_media_type_.Get(),
                               0);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set output media type", false);

  // Initialize input parameters.
  hr = MFCreateMediaType(imf_input_media_type_.GetAddressOf());
  RETURN_ON_HR_FAILURE(hr, "Couldn't create media type", false);
  hr = imf_input_media_type_->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set media type", false);
  hr = imf_input_media_type_->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YV12);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set video format", false);
  hr = MFSetAttributeRatio(imf_input_media_type_.Get(), MF_MT_FRAME_RATE,
                           frame_rate_, 1);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame rate", false);
  hr = MFSetAttributeSize(imf_input_media_type_.Get(), MF_MT_FRAME_SIZE,
                          input_visible_size_.width(),
                          input_visible_size_.height());
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame size", false);
  hr = imf_input_media_type_->SetUINT32(MF_MT_INTERLACE_MODE,
                                        MFVideoInterlace_Progressive);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set interlace mode", false);
  hr = encoder_->SetInputType(input_stream_id_, imf_input_media_type_.Get(), 0);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set input media type", false);

  return true;
}

bool MediaFoundationVideoEncodeAccelerator::SetEncoderModes() {
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());
  RETURN_ON_FAILURE((encoder_.Get() != nullptr),
                    "No HW encoder instance created", false);

  HRESULT hr = encoder_.CopyTo(codec_api_.GetAddressOf());
  RETURN_ON_HR_FAILURE(hr, "Couldn't get ICodecAPI", false);
  VARIANT var;
  var.vt = VT_UI4;
  var.ulVal = eAVEncCommonRateControlMode_CBR;
  hr = codec_api_->SetValue(&CODECAPI_AVEncCommonRateControlMode, &var);
  if (!compatible_with_win7_) {
    // Though CODECAPI_AVEncCommonRateControlMode is supported by Windows 7, but
    // according to a discussion on MSDN,
    // https://social.msdn.microsoft.com/Forums/windowsdesktop/en-US/6da521e9-7bb3-4b79-a2b6-b31509224638/win7-h264-encoder-imfsinkwriter-cant-use-quality-vbr-encoding?forum=mediafoundationdevelopment
    // setting it on Windows 7 returns error.
    RETURN_ON_HR_FAILURE(hr, "Couldn't set CommonRateControlMode", false);
  }
  var.ulVal = target_bitrate_;
  hr = codec_api_->SetValue(&CODECAPI_AVEncCommonMeanBitRate, &var);
  if (!compatible_with_win7_) {
    RETURN_ON_HR_FAILURE(hr, "Couldn't set bitrate", false);
  }
  var.ulVal = eAVEncAdaptiveMode_Resolution;
  hr = codec_api_->SetValue(&CODECAPI_AVEncAdaptiveMode, &var);
  if (!compatible_with_win7_) {
    RETURN_ON_HR_FAILURE(hr, "Couldn't set FrameRate", false);
  }
  var.vt = VT_BOOL;
  var.boolVal = VARIANT_TRUE;
  hr = codec_api_->SetValue(&CODECAPI_AVLowLatencyMode, &var);
  if (!compatible_with_win7_) {
    RETURN_ON_HR_FAILURE(hr, "Couldn't set LowLatencyMode", false);
  }

  return true;
}

bool MediaFoundationVideoEncodeAccelerator::IsResolutionSupported(
    const gfx::Size& resolution) {
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());
  DCHECK(encoder_);

  HRESULT hr =
      MFSetAttributeSize(imf_output_media_type_.Get(), MF_MT_FRAME_SIZE,
                         resolution.width(), resolution.height());
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame size", false);
  hr = encoder_->SetOutputType(output_stream_id_, imf_output_media_type_.Get(),
                               0);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set output media type", false);

  hr = MFSetAttributeSize(imf_input_media_type_.Get(), MF_MT_FRAME_SIZE,
                          resolution.width(), resolution.height());
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame size", false);
  hr = encoder_->SetInputType(input_stream_id_, imf_input_media_type_.Get(), 0);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set input media type", false);

  return true;
}

void MediaFoundationVideoEncodeAccelerator::NotifyError(
    VideoEncodeAccelerator::Error error) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread() ||
         main_client_task_runner_->BelongsToCurrentThread());

  main_client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::NotifyError, main_client_, error));
}

void MediaFoundationVideoEncodeAccelerator::EncodeTask(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe) {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  Microsoft::WRL::ComPtr<IMFMediaBuffer> input_buffer;
  input_sample_->GetBufferByIndex(0, input_buffer.GetAddressOf());

  {
    MediaBufferScopedPointer scoped_buffer(input_buffer.Get());
    DCHECK(scoped_buffer.get());
    libyuv::I420Copy(frame->visible_data(VideoFrame::kYPlane),
                     frame->stride(VideoFrame::kYPlane),
                     frame->visible_data(VideoFrame::kVPlane),
                     frame->stride(VideoFrame::kVPlane),
                     frame->visible_data(VideoFrame::kUPlane),
                     frame->stride(VideoFrame::kUPlane), scoped_buffer.get(),
                     y_stride_, scoped_buffer.get() + u_plane_offset_,
                     u_stride_, scoped_buffer.get() + v_plane_offset_,
                     v_stride_, input_visible_size_.width(),
                     input_visible_size_.height());
  }

  input_sample_->SetSampleTime(frame->timestamp().InMicroseconds() *
                               kOneMicrosecondInMFSampleTimeUnits);
  UINT64 sample_duration = 1;
  HRESULT hr =
      MFFrameRateToAverageTimePerFrame(frame_rate_, 1, &sample_duration);
  RETURN_ON_HR_FAILURE(hr, "Couldn't calculate sample duration", );
  input_sample_->SetSampleDuration(sample_duration);

  // Release frame after input is copied.
  frame = nullptr;

  if (force_keyframe) {
    VARIANT var;
    var.vt = VT_UI4;
    var.ulVal = 1;
    hr = codec_api_->SetValue(&CODECAPI_AVEncVideoForceKeyFrame, &var);
    if (!compatible_with_win7_ && !SUCCEEDED(hr)) {
      LOG(WARNING) << "Failed to set CODECAPI_AVEncVideoForceKeyFrame, "
                      "HRESULT: 0x" << std::hex << hr;
    }
  }

  hr = encoder_->ProcessInput(input_stream_id_, input_sample_.Get(), 0);
  // According to MSDN, if encoder returns MF_E_NOTACCEPTING, we need to try
  // processing the output. This error indicates that encoder does not accept
  // any more input data.
  if (hr == MF_E_NOTACCEPTING) {
    DVLOG(3) << "MF_E_NOTACCEPTING";
    ProcessOutput();
    hr = encoder_->ProcessInput(input_stream_id_, input_sample_.Get(), 0);
    if (!SUCCEEDED(hr)) {
      NotifyError(kPlatformFailureError);
      RETURN_ON_HR_FAILURE(hr, "Couldn't encode", );
    }
  } else if (!SUCCEEDED(hr)) {
    NotifyError(kPlatformFailureError);
    RETURN_ON_HR_FAILURE(hr, "Couldn't encode", );
  }
  DVLOG(3) << "Sent for encode " << hr;

  ProcessOutput();
}

void MediaFoundationVideoEncodeAccelerator::ProcessOutput() {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  DWORD output_status = 0;
  HRESULT hr = encoder_->GetOutputStatus(&output_status);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get output status", );
  if (output_status != MFT_OUTPUT_STATUS_SAMPLE_READY) {
    DVLOG(3) << "Output isnt ready";
    return;
  }

  MFT_OUTPUT_DATA_BUFFER output_data_buffer = {0};
  output_data_buffer.dwStreamID = 0;
  output_data_buffer.dwStatus = 0;
  output_data_buffer.pEvents = NULL;
  output_data_buffer.pSample = output_sample_.Get();
  DWORD status = 0;
  hr = encoder_->ProcessOutput(output_stream_id_, 1, &output_data_buffer,
                               &status);
  if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
    DVLOG(3) << "MF_E_TRANSFORM_NEED_MORE_INPUT" << status;
    return;
  }
  RETURN_ON_HR_FAILURE(hr, "Couldn't get encoded data", );
  DVLOG(3) << "Got encoded data " << hr;

  Microsoft::WRL::ComPtr<IMFMediaBuffer> output_buffer;
  hr = output_sample_->GetBufferByIndex(0, output_buffer.GetAddressOf());
  RETURN_ON_HR_FAILURE(hr, "Couldn't get buffer by index", );
  DWORD size = 0;
  hr = output_buffer->GetCurrentLength(&size);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get buffer length", );

  base::TimeDelta timestamp;
  LONGLONG sample_time;
  hr = output_sample_->GetSampleTime(&sample_time);
  if (SUCCEEDED(hr)) {
    timestamp = base::TimeDelta::FromMicroseconds(
        sample_time / kOneMicrosecondInMFSampleTimeUnits);
  }

  const bool keyframe = MFGetAttributeUINT32(
      output_sample_.Get(), MFSampleExtension_CleanPoint, false);
  DVLOG(3) << "We HAVE encoded data with size:" << size << " keyframe "
           << keyframe;

  if (bitstream_buffer_queue_.empty()) {
    DVLOG(3) << "No bitstream buffers.";
    // We need to copy the output so that encoding can continue.
    std::unique_ptr<EncodeOutput> encode_output(
        new EncodeOutput(size, keyframe, timestamp));
    {
      MediaBufferScopedPointer scoped_buffer(output_buffer.Get());
      memcpy(encode_output->memory(), scoped_buffer.get(), size);
    }
    encoder_output_queue_.push_back(std::move(encode_output));
    return;
  }

  std::unique_ptr<MediaFoundationVideoEncodeAccelerator::BitstreamBufferRef>
      buffer_ref = std::move(bitstream_buffer_queue_.front());
  bitstream_buffer_queue_.pop_front();

  {
    MediaBufferScopedPointer scoped_buffer(output_buffer.Get());
    memcpy(buffer_ref->mapping.memory(), scoped_buffer.get(), size);
  }

  main_client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Client::BitstreamBufferReady, main_client_,
                     buffer_ref->id,
                     BitstreamBufferMetadata(size, keyframe, timestamp)));

  // Keep calling ProcessOutput recursively until MF_E_TRANSFORM_NEED_MORE_INPUT
  // is returned to flush out all the output.
  ProcessOutput();
}

void MediaFoundationVideoEncodeAccelerator::UseOutputBitstreamBufferTask(
    std::unique_ptr<BitstreamBufferRef> buffer_ref) {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  // If there is already EncodeOutput waiting, copy its output first.
  if (!encoder_output_queue_.empty()) {
    std::unique_ptr<MediaFoundationVideoEncodeAccelerator::EncodeOutput>
        encode_output = std::move(encoder_output_queue_.front());
    encoder_output_queue_.pop_front();
    ReturnBitstreamBuffer(std::move(encode_output), std::move(buffer_ref));
    return;
  }

  bitstream_buffer_queue_.push_back(std::move(buffer_ref));
}

void MediaFoundationVideoEncodeAccelerator::ReturnBitstreamBuffer(
    std::unique_ptr<EncodeOutput> encode_output,
    std::unique_ptr<MediaFoundationVideoEncodeAccelerator::BitstreamBufferRef>
        buffer_ref) {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  memcpy(buffer_ref->mapping.memory(), encode_output->memory(),
         encode_output->size());
  main_client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Client::BitstreamBufferReady, main_client_,
                     buffer_ref->id,
                     BitstreamBufferMetadata(
                         encode_output->size(), encode_output->keyframe,
                         encode_output->capture_timestamp)));
}

void MediaFoundationVideoEncodeAccelerator::RequestEncodingParametersChangeTask(
    uint32_t bitrate,
    uint32_t framerate) {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  frame_rate_ =
      framerate
          ? std::min(framerate, static_cast<uint32_t>(kMaxFrameRateNumerator))
          : 1;

  if (target_bitrate_ != bitrate) {
    target_bitrate_ = bitrate ? bitrate : 1;
    VARIANT var;
    var.vt = VT_UI4;
    var.ulVal = target_bitrate_;
    HRESULT hr = codec_api_->SetValue(&CODECAPI_AVEncCommonMeanBitRate, &var);
    if (!compatible_with_win7_) {
      RETURN_ON_HR_FAILURE(hr, "Couldn't set bitrate", );
    }
  }
}

void MediaFoundationVideoEncodeAccelerator::DestroyTask() {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  // Cancel all encoder thread callbacks.
  encoder_task_weak_factory_.InvalidateWeakPtrs();

  ReleaseEncoderResources();
}

void MediaFoundationVideoEncodeAccelerator::ReleaseEncoderResources() {
  encoder_.Reset();
  codec_api_.Reset();
  imf_input_media_type_.Reset();
  imf_output_media_type_.Reset();
  input_sample_.Reset();
  output_sample_.Reset();
}

}  // namespace content
