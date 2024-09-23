// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/video_capture_device_mf_win.h"

#include <d3d11_4.h>
#include <ks.h>
#include <ksmedia.h>
#include <mfapi.h>
#include <mferror.h>
#include <stddef.h>
#include <wincodec.h>
#include <wrl/implements.h>

#include <d3d11.h>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/windows_version.h"
#include "media/base/media_switches.h"
#include "media/base/win/color_space_util_win.h"
#include "media/capture/mojom/image_capture_types.h"
#include "media/capture/video/blob_utils.h"
#include "media/capture/video/win/capability_list_win.h"
#include "media/capture/video/win/sink_filter_win.h"
#include "media/capture/video/win/video_capture_device_utils_win.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_memory_buffer.h"

using base::Location;
using base::win::ScopedCoMem;
using Microsoft::WRL::ComPtr;

namespace media {

BASE_FEATURE(kMediaFoundationVideoCaptureForwardSampleTimestamps,
             "MediaFoundationVideoCaptureForwardSampleTimestamps",
             base::FEATURE_DISABLED_BY_DEFAULT);

ULONGLONG CaptureModeToExtendedPlatformFlags(
    mojom::EyeGazeCorrectionMode mode) {
  switch (mode) {
    case mojom::EyeGazeCorrectionMode::OFF:
      return KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_OFF;
    case mojom::EyeGazeCorrectionMode::ON:
      return KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_ON;
    case mojom::EyeGazeCorrectionMode::STARE:
      return KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_ON |
             KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_STARE;
  }
  NOTREACHED();
}

mojom::EyeGazeCorrectionMode ExtendedPlatformFlagsToCaptureMode(
    ULONGLONG flags,
    mojom::EyeGazeCorrectionMode default_mode) {
  switch (flags & (KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_OFF |
                   KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_ON |
                   KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_STARE)) {
    case KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_OFF:
      return mojom::EyeGazeCorrectionMode::OFF;
    case KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_ON:
      return mojom::EyeGazeCorrectionMode::ON;
    case (KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_ON |
          KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_STARE):
      return mojom::EyeGazeCorrectionMode::STARE;
    default:
      return default_mode;
  }
}

std::vector<mojom::EyeGazeCorrectionMode> ExtendedPlatformFlagsToCaptureModes(
    ULONGLONG flags) {
  std::vector<mojom::EyeGazeCorrectionMode> modes = {
      mojom::EyeGazeCorrectionMode::OFF};
  if (flags & KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_ON) {
    modes.push_back(mojom::EyeGazeCorrectionMode::ON);
    if (flags & KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_STARE) {
      modes.push_back(mojom::EyeGazeCorrectionMode::STARE);
    }
  }
  return modes;
}

#if DCHECK_IS_ON()
#define DLOG_IF_FAILED_WITH_HRESULT(message, hr)                      \
  {                                                                   \
    DLOG_IF(ERROR, FAILED(hr))                                        \
        << (message) << ": " << logging::SystemErrorCodeToString(hr); \
  }
#else
#define DLOG_IF_FAILED_WITH_HRESULT(message, hr) \
  {}
#endif

namespace {

std::optional<base::TimeTicks> MaybeForwardCaptureBeginTime(
    const base::TimeTicks capture_begin_time) {
  if (!base::FeatureList::IsEnabled(
          kMediaFoundationVideoCaptureForwardSampleTimestamps)) {
    return std::nullopt;
  }
  return capture_begin_time;
}

// How long premapped frames will be premapped after corresponding feedback
// message is received. Too high value would cause unnecessary premapped frames
// when a VideoTrack is disconnected from the source requiring premapped frames.
// If he value is less than the frame duration, the feedback might be ignored
// completely and a costly mapping will be happening instead of the premapping.
constexpr base::TimeDelta kMaxFeedbackPremappingEffectDuration =
    base::Milliseconds(500);

// Provide an unique GUID for reusing |handle| and |token| by
// SetPrivateDataInterface/GetPrivateData.
// {79BFE1AB-CE47-4C3D-BDB2-06E6B886368C}
constexpr GUID DXGIHandlePrivateDataGUID = {
    0x79bfe1ab,
    0xce47,
    0x4c3d,
    {0xbd, 0xb2, 0x6, 0xe6, 0xb8, 0x86, 0x36, 0x8c}};

// How many times we try to restart D3D11 path.
constexpr int kMaxD3DRestarts = 2;

class MFPhotoCallback final
    : public base::RefCountedThreadSafe<MFPhotoCallback>,
      public IMFCaptureEngineOnSampleCallback {
 public:
  MFPhotoCallback(VideoCaptureDevice::TakePhotoCallback callback,
                  VideoCaptureFormat format)
      : callback_(std::move(callback)), format_(format) {}

  MFPhotoCallback(const MFPhotoCallback&) = delete;
  MFPhotoCallback& operator=(const MFPhotoCallback&) = delete;

  IFACEMETHODIMP QueryInterface(REFIID riid, void** object) override {
    if (riid == IID_IUnknown || riid == IID_IMFCaptureEngineOnSampleCallback) {
      AddRef();
      *object = static_cast<IMFCaptureEngineOnSampleCallback*>(this);
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  IFACEMETHODIMP_(ULONG) AddRef() override {
    base::RefCountedThreadSafe<MFPhotoCallback>::AddRef();
    return 1U;
  }

  IFACEMETHODIMP_(ULONG) Release() override {
    base::RefCountedThreadSafe<MFPhotoCallback>::Release();
    return 1U;
  }

  IFACEMETHODIMP OnSample(IMFSample* sample) override {
    if (!sample)
      return S_OK;

    DWORD buffer_count = 0;
    sample->GetBufferCount(&buffer_count);

    for (DWORD i = 0; i < buffer_count; ++i) {
      ComPtr<IMFMediaBuffer> buffer;
      sample->GetBufferByIndex(i, &buffer);
      if (!buffer)
        continue;

      BYTE* data = nullptr;
      DWORD max_length = 0;
      DWORD length = 0;
      buffer->Lock(&data, &max_length, &length);
      mojom::BlobPtr blob = RotateAndBlobify(data, length, format_, 0);
      buffer->Unlock();
      if (blob) {
        std::move(callback_).Run(std::move(blob));

        // What is it supposed to mean if there is more than one buffer sent to
        // us as a response to requesting a single still image? Are we supposed
        // to somehow concatenate the buffers? Or is it safe to ignore extra
        // buffers? For now, we ignore extra buffers.
        break;
      }
    }
    return S_OK;
  }

 private:
  friend class base::RefCountedThreadSafe<MFPhotoCallback>;
  ~MFPhotoCallback() = default;

  VideoCaptureDevice::TakePhotoCallback callback_;
  const VideoCaptureFormat format_;
};

// Locks the given buffer using the fastest supported method when constructed,
// and automatically unlocks the buffer when destroyed.
class ScopedBufferLock {
 public:
  explicit ScopedBufferLock(ComPtr<IMFMediaBuffer> buffer)
      : buffer_(std::move(buffer)) {
    if (FAILED(buffer_.As(&buffer_2d_))) {
      LockSlow();
      return;
    }
    // Try lock methods from fastest to slowest: Lock2DSize(), then Lock2D(),
    // then finally LockSlow().
    if (Lock2DSize() || Lock2D()) {
      if (IsContiguous())
        return;
      buffer_2d_->Unlock2D();
    }
    // Fall back to LockSlow() if 2D buffer was unsupported or noncontiguous.
    buffer_2d_ = nullptr;
    LockSlow();
  }

  // Returns whether |buffer_2d_| is contiguous with positive pitch, i.e., the
  // buffer format that the surrounding code expects.
  bool IsContiguous() {
    BOOL is_contiguous;
    return pitch_ > 0 &&
           SUCCEEDED(buffer_2d_->IsContiguousFormat(&is_contiguous)) &&
           is_contiguous &&
           (length_ || SUCCEEDED(buffer_2d_->GetContiguousLength(&length_)));
  }

  bool Lock2DSize() {
    ComPtr<IMF2DBuffer2> buffer_2d_2;
    if (FAILED(buffer_.As(&buffer_2d_2)))
      return false;
    BYTE* data_start;
    return SUCCEEDED(buffer_2d_2->Lock2DSize(MF2DBuffer_LockFlags_Read, &data_,
                                             &pitch_, &data_start, &length_));
  }

  bool Lock2D() { return SUCCEEDED(buffer_2d_->Lock2D(&data_, &pitch_)); }

  void LockSlow() {
    DWORD max_length = 0;
    buffer_->Lock(&data_, &max_length, &length_);
  }

  ~ScopedBufferLock() {
    if (buffer_2d_)
      buffer_2d_->Unlock2D();
    else
      buffer_->Unlock();
  }

  ScopedBufferLock(const ScopedBufferLock&) = delete;
  ScopedBufferLock& operator=(const ScopedBufferLock&) = delete;

  BYTE* data() const { return data_; }
  DWORD length() const { return length_; }

 private:
  ComPtr<IMFMediaBuffer> buffer_;
  ComPtr<IMF2DBuffer> buffer_2d_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION BYTE* data_ = nullptr;
  DWORD length_ = 0;
  LONG pitch_ = 0;
};

scoped_refptr<IMFCaptureEngineOnSampleCallback> CreateMFPhotoCallback(
    VideoCaptureDevice::TakePhotoCallback callback,
    VideoCaptureFormat format) {
  return scoped_refptr<IMFCaptureEngineOnSampleCallback>(
      new MFPhotoCallback(std::move(callback), format));
}

void LogError(const Location& from_here, HRESULT hr) {
  LOG(ERROR) << from_here.ToString()
             << " hr = " << logging::SystemErrorCodeToString(hr);
}

bool GetFrameSizeFromMediaType(IMFMediaType* type, gfx::Size* frame_size) {
  UINT32 width32, height32;
  if (FAILED(MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &width32, &height32)))
    return false;
  frame_size->SetSize(width32, height32);
  return true;
}

bool GetFrameRateFromMediaType(IMFMediaType* type, float* frame_rate) {
  UINT32 numerator, denominator;
  if (FAILED(MFGetAttributeRatio(type, MF_MT_FRAME_RATE, &numerator,
                                 &denominator)) ||
      !denominator) {
    return false;
  }
  *frame_rate = static_cast<float>(numerator) / denominator;
  return true;
}

struct PixelFormatMap {
  GUID mf_source_media_subtype;
  VideoPixelFormat pixel_format;
};

VideoPixelFormat MfSubTypeToSourcePixelFormat(
    const GUID& mf_source_media_subtype) {
  static const PixelFormatMap kPixelFormatMap[] = {

      {MFVideoFormat_I420, PIXEL_FORMAT_I420},
      {MFVideoFormat_YUY2, PIXEL_FORMAT_YUY2},
      {MFVideoFormat_UYVY, PIXEL_FORMAT_UYVY},
      {MFVideoFormat_RGB24, PIXEL_FORMAT_RGB24},
      {MFVideoFormat_RGB32, PIXEL_FORMAT_XRGB},
      {MFVideoFormat_ARGB32, PIXEL_FORMAT_ARGB},
      {MFVideoFormat_MJPG, PIXEL_FORMAT_MJPEG},
      {MFVideoFormat_NV12, PIXEL_FORMAT_NV12},
      {MFVideoFormat_YV12, PIXEL_FORMAT_YV12},
      {GUID_ContainerFormatJpeg, PIXEL_FORMAT_MJPEG}};

  for (const auto& [source_media_subtype, pixel_format] : kPixelFormatMap) {
    if (source_media_subtype == mf_source_media_subtype) {
      return pixel_format;
    }
  }
  return PIXEL_FORMAT_UNKNOWN;
}

bool GetFormatFromSourceMediaType(IMFMediaType* source_media_type,
                                  bool photo,
                                  bool use_hardware_format,
                                  VideoCaptureFormat* format,
                                  VideoPixelFormat* source_pixel_format) {
  GUID major_type_guid;
  if (FAILED(source_media_type->GetGUID(MF_MT_MAJOR_TYPE, &major_type_guid)) ||
      (major_type_guid != MFMediaType_Image &&
       (photo ||
        !GetFrameRateFromMediaType(source_media_type, &format->frame_rate)))) {
    return false;
  }

  GUID sub_type_guid;
  if (FAILED(source_media_type->GetGUID(MF_MT_SUBTYPE, &sub_type_guid)) ||
      !GetFrameSizeFromMediaType(source_media_type, &format->frame_size) ||
      !VideoCaptureDeviceMFWin::GetPixelFormatFromMFSourceMediaSubtype(
          sub_type_guid, use_hardware_format, &format->pixel_format)) {
    return false;
  }

  *source_pixel_format = MfSubTypeToSourcePixelFormat(sub_type_guid);
  return true;
}

HRESULT CopyAttribute(IMFAttributes* source_attributes,
                      IMFAttributes* destination_attributes,
                      const GUID& key) {
  PROPVARIANT var;
  PropVariantInit(&var);
  HRESULT hr = source_attributes->GetItem(key, &var);
  if (FAILED(hr))
    return hr;

  hr = destination_attributes->SetItem(key, var);
  PropVariantClear(&var);
  return hr;
}

struct MediaFormatConfiguration {
  bool is_hardware_format;
  GUID mf_source_media_subtype;
  GUID mf_sink_media_subtype;
  VideoPixelFormat pixel_format;
};

bool GetMediaFormatConfigurationFromMFSourceMediaSubtype(
    const GUID& mf_source_media_subtype,
    bool use_hardware_format,
    MediaFormatConfiguration* media_format_configuration) {
  static const MediaFormatConfiguration kMediaFormatConfigurationMap[] = {
      // IMFCaptureEngine inevitably performs the video frame decoding itself.
      // This means that the sink must always be set to an uncompressed video
      // format.

      // Since chromium uses I420 at the other end of the pipe, MF known video
      // output formats are always set to I420.
      {false, MFVideoFormat_I420, MFVideoFormat_I420, PIXEL_FORMAT_I420},
      {false, MFVideoFormat_YUY2, MFVideoFormat_I420, PIXEL_FORMAT_I420},
      {false, MFVideoFormat_UYVY, MFVideoFormat_I420, PIXEL_FORMAT_I420},
      {false, MFVideoFormat_RGB24, MFVideoFormat_I420, PIXEL_FORMAT_I420},
      {false, MFVideoFormat_RGB32, MFVideoFormat_I420, PIXEL_FORMAT_I420},
      {false, MFVideoFormat_ARGB32, MFVideoFormat_I420, PIXEL_FORMAT_I420},
      {false, MFVideoFormat_MJPG, MFVideoFormat_I420, PIXEL_FORMAT_I420},
      {false, MFVideoFormat_NV12, MFVideoFormat_I420, PIXEL_FORMAT_I420},
      {false, MFVideoFormat_YV12, MFVideoFormat_I420, PIXEL_FORMAT_I420},

      // Depth cameras use 16-bit uncompressed video formats.
      // We ask IMFCaptureEngine to let the frame pass through, without
      // transcoding, since transcoding would lead to precision loss.
      {false, kMediaSubTypeY16, kMediaSubTypeY16, PIXEL_FORMAT_Y16},
      {false, kMediaSubTypeZ16, kMediaSubTypeZ16, PIXEL_FORMAT_Y16},
      {false, kMediaSubTypeINVZ, kMediaSubTypeINVZ, PIXEL_FORMAT_Y16},
      {false, MFVideoFormat_D16, MFVideoFormat_D16, PIXEL_FORMAT_Y16},

      // Photo type
      {false, GUID_ContainerFormatJpeg, GUID_ContainerFormatJpeg,
       PIXEL_FORMAT_MJPEG},

      // For hardware path we always convert to NV12, since it's the only
      // supported by GMBs format.
      {true, MFVideoFormat_I420, MFVideoFormat_NV12, PIXEL_FORMAT_NV12},
      {true, MFVideoFormat_YUY2, MFVideoFormat_NV12, PIXEL_FORMAT_NV12},
      {true, MFVideoFormat_UYVY, MFVideoFormat_NV12, PIXEL_FORMAT_NV12},
      {true, MFVideoFormat_RGB24, MFVideoFormat_NV12, PIXEL_FORMAT_NV12},
      {true, MFVideoFormat_RGB32, MFVideoFormat_NV12, PIXEL_FORMAT_NV12},
      {true, MFVideoFormat_ARGB32, MFVideoFormat_NV12, PIXEL_FORMAT_NV12},
      {true, MFVideoFormat_MJPG, MFVideoFormat_NV12, PIXEL_FORMAT_NV12},
      {true, MFVideoFormat_NV12, MFVideoFormat_NV12, PIXEL_FORMAT_NV12},
      {true, MFVideoFormat_YV12, MFVideoFormat_NV12, PIXEL_FORMAT_NV12},

      // 16-bit formats can't be converted without loss of precision,
      // so if leave an option to get Y16 pixel format even though the
      // HW path won't be used for it.
      {true, kMediaSubTypeY16, kMediaSubTypeY16, PIXEL_FORMAT_Y16},
      {true, kMediaSubTypeZ16, kMediaSubTypeZ16, PIXEL_FORMAT_Y16},
      {true, kMediaSubTypeINVZ, kMediaSubTypeINVZ, PIXEL_FORMAT_Y16},
      {true, MFVideoFormat_D16, MFVideoFormat_D16, PIXEL_FORMAT_Y16},

      // Photo type
      {true, GUID_ContainerFormatJpeg, GUID_ContainerFormatJpeg,
       PIXEL_FORMAT_MJPEG}};

  for (const auto& kMediaFormatConfiguration : kMediaFormatConfigurationMap) {
    if (kMediaFormatConfiguration.is_hardware_format == use_hardware_format &&
        kMediaFormatConfiguration.mf_source_media_subtype ==
            mf_source_media_subtype) {
      *media_format_configuration = kMediaFormatConfiguration;
      return true;
    }
  }

  return false;
}

// Calculate sink subtype based on source subtype. |passthrough| is set when
// sink and source are the same and means that there should be no transcoding
// done by IMFCaptureEngine.
HRESULT GetMFSinkMediaSubtype(IMFMediaType* source_media_type,
                              bool use_hardware_format,
                              GUID* mf_sink_media_subtype,
                              bool* passthrough) {
  GUID source_subtype;
  HRESULT hr = source_media_type->GetGUID(MF_MT_SUBTYPE, &source_subtype);
  if (FAILED(hr))
    return hr;
  MediaFormatConfiguration media_format_configuration;
  if (!GetMediaFormatConfigurationFromMFSourceMediaSubtype(
          source_subtype, use_hardware_format, &media_format_configuration))
    return E_FAIL;
  *mf_sink_media_subtype = media_format_configuration.mf_sink_media_subtype;
  *passthrough =
      (media_format_configuration.mf_sink_media_subtype == source_subtype);
  return S_OK;
}

HRESULT ConvertToPhotoSinkMediaType(IMFMediaType* source_media_type,
                                    IMFMediaType* destination_media_type) {
  HRESULT hr =
      destination_media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Image);
  if (FAILED(hr))
    return hr;

  bool passthrough = false;
  GUID mf_sink_media_subtype;
  hr = GetMFSinkMediaSubtype(source_media_type, /*use_hardware_format=*/false,
                             &mf_sink_media_subtype, &passthrough);
  if (FAILED(hr))
    return hr;

  hr = destination_media_type->SetGUID(MF_MT_SUBTYPE, mf_sink_media_subtype);
  if (FAILED(hr))
    return hr;

  return CopyAttribute(source_media_type, destination_media_type,
                       MF_MT_FRAME_SIZE);
}

HRESULT ConvertToVideoSinkMediaType(IMFMediaType* source_media_type,
                                    bool use_hardware_format,
                                    IMFMediaType* sink_media_type) {
  HRESULT hr = sink_media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  if (FAILED(hr))
    return hr;

  bool passthrough = false;
  GUID mf_sink_media_subtype;
  hr = GetMFSinkMediaSubtype(source_media_type, use_hardware_format,
                             &mf_sink_media_subtype, &passthrough);
  if (FAILED(hr))
    return hr;

  hr = sink_media_type->SetGUID(MF_MT_SUBTYPE, mf_sink_media_subtype);
  // Copying attribute values for passthrough mode is redundant, since the
  // format is kept unchanged, and causes AddStream error MF_E_INVALIDMEDIATYPE.
  if (FAILED(hr) || passthrough)
    return hr;

  // Since we have the option to send video color space at RTP layer, copy the
  // nominal range attribute from source to sink instead of rewriting it to
  // limited range. See https://crbug.com/1449570 for more details.
  if (base::FeatureList::IsEnabled(media::kWebRTCColorAccuracy)) {
    hr = CopyAttribute(source_media_type, sink_media_type,
                       MF_MT_VIDEO_NOMINAL_RANGE);
  } else {
    hr = sink_media_type->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE,
                                    MFNominalRange_16_235);
  }
  if (FAILED(hr))
    return hr;

  // Next three attributes may be missing, unless a HDR video is captured so
  // ignore errors.
  CopyAttribute(source_media_type, sink_media_type, MF_MT_VIDEO_PRIMARIES);
  CopyAttribute(source_media_type, sink_media_type, MF_MT_TRANSFER_FUNCTION);
  CopyAttribute(source_media_type, sink_media_type, MF_MT_YUV_MATRIX);

  hr = CopyAttribute(source_media_type, sink_media_type, MF_MT_FRAME_SIZE);
  if (FAILED(hr))
    return hr;

  hr = CopyAttribute(source_media_type, sink_media_type, MF_MT_FRAME_RATE);
  if (FAILED(hr))
    return hr;

  hr = CopyAttribute(source_media_type, sink_media_type,
                     MF_MT_PIXEL_ASPECT_RATIO);
  if (FAILED(hr))
    return hr;

  return CopyAttribute(source_media_type, sink_media_type,
                       MF_MT_INTERLACE_MODE);
}

const CapabilityWin& GetBestMatchedPhotoCapability(
    ComPtr<IMFMediaType> current_media_type,
    gfx::Size requested_size,
    const CapabilityList& capabilities) {
  gfx::Size current_size;
  GetFrameSizeFromMediaType(current_media_type.Get(), &current_size);

  int requested_height = requested_size.height() > 0 ? requested_size.height()
                                                     : current_size.height();
  int requested_width = requested_size.width() > 0 ? requested_size.width()
                                                   : current_size.width();

  const CapabilityWin* best_match = &(*capabilities.begin());
  for (const CapabilityWin& capability : capabilities) {
    int height = capability.supported_format.frame_size.height();
    int width = capability.supported_format.frame_size.width();
    int best_height = best_match->supported_format.frame_size.height();
    int best_width = best_match->supported_format.frame_size.width();

    if (std::abs(height - requested_height) <= std::abs(height - best_height) &&
        std::abs(width - requested_width) <= std::abs(width - best_width)) {
      best_match = &capability;
    }
  }
  return *best_match;
}

HRESULT CreateCaptureEngine(IMFCaptureEngine** engine) {
  ComPtr<IMFCaptureEngineClassFactory> capture_engine_class_factory;
  HRESULT hr = CoCreateInstance(CLSID_MFCaptureEngineClassFactory, nullptr,
                                CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&capture_engine_class_factory));
  if (FAILED(hr))
    return hr;

  return capture_engine_class_factory->CreateInstance(CLSID_MFCaptureEngine,
                                                      IID_PPV_ARGS(engine));
}

bool GetCameraControlSupport(ComPtr<IAMCameraControl> camera_control,
                             CameraControlProperty control_property) {
  long min, max, step, default_value, flags;
  HRESULT hr = camera_control->GetRange(control_property, &min, &max, &step,
                                        &default_value, &flags);
  return SUCCEEDED(hr) && min < max;
}

// Retrieves the control range and value, and
// optionally returns the associated supported and current mode.
template <typename ControlInterface, typename ControlProperty>
mojom::RangePtr RetrieveControlRangeAndCurrent(
    ComPtr<ControlInterface>& control_interface,
    ControlProperty control_property,
    std::vector<mojom::MeteringMode>* supported_modes = nullptr,
    mojom::MeteringMode* current_mode = nullptr,
    double (*value_converter)(long) = PlatformToCaptureValue,
    double (*step_converter)(long, double, double) = PlatformToCaptureStep) {
  return media::RetrieveControlRangeAndCurrent(
      [&control_interface, control_property](auto... args) {
        return control_interface->GetRange(control_property, args...);
      },
      [&control_interface, control_property](auto... args) {
        return control_interface->Get(control_property, args...);
      },
      supported_modes, current_mode, value_converter, step_converter);
}

HRESULT GetTextureFromMFBuffer(IMFMediaBuffer* mf_buffer,
                               ID3D11Texture2D** texture_out) {
  Microsoft::WRL::ComPtr<IMFDXGIBuffer> dxgi_buffer;
  HRESULT hr = mf_buffer->QueryInterface(IID_PPV_ARGS(&dxgi_buffer));
  DLOG_IF_FAILED_WITH_HRESULT("Failed to retrieve IMFDXGIBuffer", hr);

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d_texture;
  if (SUCCEEDED(hr)) {
    hr = dxgi_buffer->GetResource(IID_PPV_ARGS(&d3d_texture));
    DLOG_IF_FAILED_WITH_HRESULT("Failed to retrieve ID3D11Texture2D", hr);
  }

  *texture_out = d3d_texture.Detach();
  if (SUCCEEDED(hr)) {
    CHECK(*texture_out);
  }
  return hr;
}

void GetTextureInfo(ID3D11Texture2D* texture,
                    gfx::Size& size,
                    VideoPixelFormat& format,
                    bool& is_cross_process_shared_texture) {
  D3D11_TEXTURE2D_DESC desc;
  texture->GetDesc(&desc);
  size.set_width(desc.Width);
  size.set_height(desc.Height);

  switch (desc.Format) {
    // Only support NV12
    case DXGI_FORMAT_NV12:
      format = PIXEL_FORMAT_NV12;
      break;
    default:
      DLOG(ERROR) << "Unsupported camera DXGI texture format: " << desc.Format;
      format = PIXEL_FORMAT_UNKNOWN;
      break;
  }

  // According to
  // https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_resource_misc_flag,
  // D3D11_RESOURCE_MISC_RESTRICT_SHARED_RESOURCE and
  // D3D11_RESOURCE_MISC_RESTRICT_SHARED_RESOURCE_DRIVER only support shared
  // texture on same process.
  is_cross_process_shared_texture =
      (desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED) &&
      (desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE) &&
      !(desc.MiscFlags & D3D11_RESOURCE_MISC_RESTRICT_SHARED_RESOURCE) &&
      !(desc.MiscFlags & D3D11_RESOURCE_MISC_RESTRICT_SHARED_RESOURCE_DRIVER);

  // Log this in an UMA histogram to determine what proportion of frames might
  // actually benefit from zero-copy.
  base::UmaHistogramBoolean("Media.VideoCapture.Win.Device.IsSharedTexture",
                            is_cross_process_shared_texture);
}

HRESULT CopyTextureToGpuMemoryBuffer(ID3D11Texture2D* texture,
                                     HANDLE dxgi_handle) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "CopyTextureToGpuMemoryBuffer");
  Microsoft::WRL::ComPtr<ID3D11Device> texture_device;
  texture->GetDevice(&texture_device);

  Microsoft::WRL::ComPtr<ID3D11Device1> device1;
  HRESULT hr = texture_device.As(&device1);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get ID3D11Device1: "
                << logging::SystemErrorCodeToString(hr);
    return hr;
  }

  // Open shared resource from GpuMemoryBuffer on source texture D3D11 device
  Microsoft::WRL::ComPtr<ID3D11Texture2D> target_texture;
  hr = device1->OpenSharedResource1(dxgi_handle, IID_PPV_ARGS(&target_texture));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to open shared camera target texture: "
                << logging::SystemErrorCodeToString(hr);
    return hr;
  }

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
  texture_device->GetImmediateContext(&device_context);

  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex;
  hr = target_texture.As(&keyed_mutex);
  CHECK(SUCCEEDED(hr));

  hr = keyed_mutex->AcquireSync(0, INFINITE);
  // Can't check for FAILED(hr) because AcquireSync may return e.g.
  // WAIT_ABANDONED.
  if (hr != S_OK) {
    DLOG(ERROR) << "Failed to acquire the mutex:"
                << logging::SystemErrorCodeToString(hr);
    return E_FAIL;
  }
  device_context->CopySubresourceRegion(target_texture.Get(), 0, 0, 0, 0,
                                        texture, 0, nullptr);
  keyed_mutex->ReleaseSync(0);

  // Need to flush context to ensure that other devices receive updated contents
  // of shared resource
  device_context->Flush();

  return S_OK;
}

// Destruction helper. Can't use base::DoNothingAs<> since ComPtr isn't POD.
void DestroyCaptureEngine(Microsoft::WRL::ComPtr<IMFCaptureEngine>) {}

class DXGIHandlePrivateData
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUnknown> {
 public:
  explicit DXGIHandlePrivateData(base::win::ScopedHandle texture_handle)
      : texture_handle_(std::move(texture_handle)) {}

  gfx::DXGIHandleToken GetDXGIToken() { return dxgi_token_; }
  HANDLE GetTextureHandle() { return texture_handle_.get(); }

 private:
  gfx::DXGIHandleToken dxgi_token_;
  const base::win::ScopedHandle texture_handle_;
  ~DXGIHandlePrivateData() override = default;
};

void RecordErrorHistogram(HRESULT hr) {
  base::UmaHistogramSparse("Media.VideoCapture.Win.ErrorEvent", hr);
}

}  // namespace

class VideoCaptureDeviceMFWin::MFVideoCallback final
    : public base::RefCountedThreadSafe<MFVideoCallback>,
      public IMFCameraControlNotify,
      public IMFCaptureEngineOnSampleCallback,
      public IMFCaptureEngineOnEventCallback {
 public:
  MFVideoCallback(VideoCaptureDeviceMFWin* observer) : observer_(observer) {}

  IFACEMETHODIMP QueryInterface(REFIID riid, void** object) override {
    HRESULT hr = E_NOINTERFACE;
    if (riid == IID_IUnknown) {
      *object = this;
      hr = S_OK;
    } else if (riid == IID_IMFCameraControlNotify) {
      *object = static_cast<IMFCameraControlNotify*>(this);
      hr = S_OK;
    } else if (riid == IID_IMFCaptureEngineOnSampleCallback) {
      *object = static_cast<IMFCaptureEngineOnSampleCallback*>(this);
      hr = S_OK;
    } else if (riid == IID_IMFCaptureEngineOnEventCallback) {
      *object = static_cast<IMFCaptureEngineOnEventCallback*>(this);
      hr = S_OK;
    }
    if (SUCCEEDED(hr))
      AddRef();

    return hr;
  }

  IFACEMETHODIMP_(ULONG) AddRef() override {
    base::RefCountedThreadSafe<MFVideoCallback>::AddRef();
    return 1U;
  }

  IFACEMETHODIMP_(ULONG) Release() override {
    base::RefCountedThreadSafe<MFVideoCallback>::Release();
    return 1U;
  }

  IFACEMETHODIMP_(void) OnChange(REFGUID control_set, UINT32 id) override {
    base::AutoLock lock(lock_);
    if (!observer_) {
      return;
    }
    observer_->OnCameraControlChange(control_set, id);
  }

  IFACEMETHODIMP_(void) OnError(HRESULT status) override {
    base::AutoLock lock(lock_);
    if (!observer_) {
      return;
    }
    observer_->OnCameraControlError(status);
  }

  IFACEMETHODIMP OnEvent(IMFMediaEvent* media_event) override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                 "MFVideoCallback::OnEvent");
    base::AutoLock lock(lock_);
    if (!observer_) {
      return S_OK;
    }
    observer_->OnEvent(media_event);
    return S_OK;
  }

  IFACEMETHODIMP OnSample(IMFSample* sample) override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                 "MFVideoCallback::OnSample");
    base::AutoLock lock(lock_);
    if (!observer_) {
      return S_OK;
    }
    if (!sample) {
      observer_->OnFrameDropped(
          VideoCaptureFrameDropReason::kWinMediaFoundationReceivedSampleIsNull);
      return S_OK;
    }

    base::TimeTicks reference_time(base::TimeTicks::Now());
    base::TimeTicks mf_time_now =
        base::TimeTicks() + base::Microseconds(MFGetSystemTime() / 10);

    LONGLONG raw_time_stamp = 0;
    sample->GetSampleTime(&raw_time_stamp);
    base::TimeDelta timestamp = base::Microseconds(raw_time_stamp / 10);

    uint64_t raw_capture_begin_time = 0;
    HRESULT hr = sample->GetUINT64(MFSampleExtension_DeviceReferenceSystemTime,
                                   &raw_capture_begin_time);
    if (FAILED(hr)) {
      hr = sample->GetUINT64(MFSampleExtension_DeviceReferenceSystemTime,
                             &raw_capture_begin_time);
    }
    if (FAILED(hr)) {
      raw_capture_begin_time = MFGetSystemTime();
    }

    base::TimeDelta mf_time_offset = reference_time - mf_time_now;
    base::TimeTicks capture_begin_time =
        base::TimeTicks() + base::Microseconds(raw_capture_begin_time / 10) +
        mf_time_offset;

    base::UmaHistogramCustomTimes(
        "Media.VideoCapture.Win.Device.CaptureBeginTime.MFTimeOffset",
        mf_time_offset + base::Milliseconds(150), base::Milliseconds(0),
        base::Milliseconds(299), 50);
    if (last_capture_begin_time_ > base::TimeTicks()) {
      base::UmaHistogramCustomTimes(
          "Media.VideoCapture.Win.Device.CaptureBeginTime.Interval",
          capture_begin_time - last_capture_begin_time_ +
              base::Milliseconds(150),
          base::Milliseconds(0), base::Milliseconds(299), 50);
    }

    if (capture_begin_time <= last_capture_begin_time_) {
      capture_begin_time = last_capture_begin_time_ + base::Microseconds(1);
    }
    last_capture_begin_time_ = capture_begin_time;

    DWORD count = 0;
    sample->GetBufferCount(&count);

    for (DWORD i = 0; i < count; ++i) {
      ComPtr<IMFMediaBuffer> buffer;
      sample->GetBufferByIndex(i, &buffer);
      if (buffer) {
        observer_->OnIncomingCapturedData(buffer, reference_time, timestamp,
                                          capture_begin_time);
      } else {
        observer_->OnFrameDropped(
            VideoCaptureFrameDropReason::
                kWinMediaFoundationGetBufferByIndexReturnedNull);
      }
    }
    return S_OK;
  }

  void Shutdown() {
    base::AutoLock lock(lock_);
    observer_ = nullptr;
  }

 private:
  friend class base::RefCountedThreadSafe<MFVideoCallback>;
  ~MFVideoCallback() {}

  base::TimeTicks last_capture_begin_time_ = base::TimeTicks();

  // Protects access to |observer_|.
  base::Lock lock_;
  raw_ptr<VideoCaptureDeviceMFWin> observer_ GUARDED_BY(lock_);
};

class VideoCaptureDeviceMFWin::MFActivitiesReportCallback final
    : public base::RefCountedThreadSafe<MFActivitiesReportCallback>,
      public IMFSensorActivitiesReportCallback {
 public:
  MFActivitiesReportCallback(
      base::WeakPtr<VideoCaptureDeviceMFWin> observer,
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
      std::string device_id)
      : observer_(std::move(observer)),
        main_thread_task_runner_(main_thread_task_runner),
        device_id_(device_id) {}

  IFACEMETHODIMP QueryInterface(REFIID riid, void** object) override {
    HRESULT hr = E_NOINTERFACE;
    if (riid == IID_IUnknown) {
      *object = this;
      hr = S_OK;
    } else if (riid == IID_IMFSensorActivitiesReportCallback) {
      *object = static_cast<IMFSensorActivitiesReportCallback*>(this);
      hr = S_OK;
    }
    if (SUCCEEDED(hr)) {
      AddRef();
    }

    return hr;
  }

  IFACEMETHODIMP_(ULONG) AddRef() override {
    base::RefCountedThreadSafe<MFActivitiesReportCallback>::AddRef();
    return 1U;
  }

  IFACEMETHODIMP_(ULONG) Release() override {
    base::RefCountedThreadSafe<MFActivitiesReportCallback>::Release();
    return 1U;
  }

  IFACEMETHODIMP_(HRESULT)
  OnActivitiesReport(
      IMFSensorActivitiesReport* sensorActivitiesReport) override {
    bool in_use = false;

    ComPtr<IMFSensorActivityReport> activity_report;
    HRESULT hr = sensorActivitiesReport->GetActivityReportByDeviceName(
        base::SysUTF8ToWide(device_id_).c_str(), &activity_report);
    if (FAILED(hr)) {
      return S_OK;
    }
    unsigned long proc_cnt = 0;
    hr = activity_report->GetProcessCount(&proc_cnt);
    // There can be several callback calls, some with empty process list. Ignore
    // these.
    if (FAILED(hr) || proc_cnt == 0) {
      return S_OK;
    }

    for (size_t idx = 0; idx < proc_cnt; ++idx) {
      ComPtr<IMFSensorProcessActivity> process_activity;
      hr = activity_report->GetProcessActivity(idx, &process_activity);
      if (SUCCEEDED(hr)) {
        BOOL streaming_state = false;
        hr = process_activity->GetStreamingState(&streaming_state);
        in_use |= SUCCEEDED(hr) && streaming_state;
      }
    }

    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoCaptureDeviceMFWin::OnCameraInUseReport, observer_,
                       in_use, /*is_default_action=*/false));
    return S_OK;
  }

 private:
  friend class base::RefCountedThreadSafe<MFActivitiesReportCallback>;
  ~MFActivitiesReportCallback() {}

  base::WeakPtr<VideoCaptureDeviceMFWin> observer_;
  scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;
  std::string device_id_;
};

// static
bool VideoCaptureDeviceMFWin::GetPixelFormatFromMFSourceMediaSubtype(
    const GUID& mf_source_media_subtype,
    bool use_hardware_format,
    VideoPixelFormat* pixel_format) {
  MediaFormatConfiguration media_format_configuration;
  if (!GetMediaFormatConfigurationFromMFSourceMediaSubtype(
          mf_source_media_subtype, use_hardware_format,
          &media_format_configuration))
    return false;

  *pixel_format = media_format_configuration.pixel_format;
  return true;
}

// Check if the video capture device supports pan, tilt and zoom controls.
// static
VideoCaptureControlSupport VideoCaptureDeviceMFWin::GetControlSupport(
    ComPtr<IMFMediaSource> source) {
  VideoCaptureControlSupport control_support;

  ComPtr<IAMCameraControl> camera_control;
  [[maybe_unused]] HRESULT hr = source.As(&camera_control);
  DLOG_IF_FAILED_WITH_HRESULT("Failed to retrieve IAMCameraControl", hr);
  ComPtr<IAMVideoProcAmp> video_control;
  hr = source.As(&video_control);
  DLOG_IF_FAILED_WITH_HRESULT("Failed to retrieve IAMVideoProcAmp", hr);

  // On Windows platform, some Image Capture video constraints and settings are
  // get or set using IAMCameraControl interface while the rest are get or set
  // using IAMVideoProcAmp interface and most device drivers define both of
  // them. So for simplicity GetPhotoState and SetPhotoState support Image
  // Capture API constraints and settings only if both interfaces are available.
  // Therefore, if either of these interface is missing, this backend does not
  // really support pan, tilt nor zoom.
  if (camera_control && video_control) {
    control_support.pan =
        GetCameraControlSupport(camera_control, CameraControl_Pan);
    control_support.tilt =
        GetCameraControlSupport(camera_control, CameraControl_Tilt);
    control_support.zoom =
        GetCameraControlSupport(camera_control, CameraControl_Zoom);
  }

  return control_support;
}

bool VideoCaptureDeviceMFWin::CreateMFCameraControlMonitor() {
  DCHECK(video_callback_);

  if (base::win::GetVersion() < base::win::Version::WIN11_22H2) {
    return false;
  }

  // The MF DLLs have been loaded by VideoCaptureDeviceFactoryWin.
  // Just get a DLL module handle here, once.
  static const HMODULE module = GetModuleHandleW(L"mfsensorgroup.dll");
  if (!module) {
    DLOG(ERROR) << "Failed to get the mfsensorgroup.dll module handle";
    return false;
  }
  using MFCreateCameraControlMonitorType =
      decltype(&MFCreateCameraControlMonitor);
  static const MFCreateCameraControlMonitorType create_camera_control_monitor =
      reinterpret_cast<MFCreateCameraControlMonitorType>(
          GetProcAddress(module, "MFCreateCameraControlMonitor"));
  if (!create_camera_control_monitor) {
    DLOG(ERROR) << "Failed to get the MFCreateCameraControlMonitor function";
    return false;
  }

  ComPtr<IMFCameraControlMonitor> camera_control_monitor;
  HRESULT hr = create_camera_control_monitor(
      base::SysUTF8ToWide(device_descriptor_.device_id).c_str(),
      video_callback_.get(), &camera_control_monitor);
  if (!camera_control_monitor) {
    LOG(ERROR) << "Failed to create IMFCameraControlMonitor: "
               << logging::SystemErrorCodeToString(hr);
    return false;
  }
  hr = camera_control_monitor->AddControlSubscription(
      KSPROPERTYSETID_ANYCAMERACONTROL, 0);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to add IMFCameraControlMonitor control subscription: "
               << logging::SystemErrorCodeToString(hr);
    return false;
  }
  hr = camera_control_monitor->Start();
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to start IMFCameraControlMonitor: "
               << logging::SystemErrorCodeToString(hr);
    return false;
  }
  camera_control_monitor_ = std::move(camera_control_monitor);
  return true;
}

HRESULT VideoCaptureDeviceMFWin::ExecuteHresultCallbackWithRetries(
    base::RepeatingCallback<HRESULT()> callback,
    MediaFoundationFunctionRequiringRetry which_function) {
  // Retry callback execution on MF_E_INVALIDREQUEST.
  // MF_E_INVALIDREQUEST is not documented in MediaFoundation documentation.
  // It could mean that MediaFoundation or the underlying device can be in a
  // state that reject these calls. Since MediaFoundation gives no intel about
  // that state beginning and ending (i.e. via some kind of event), we retry the
  // call until it succeed.
  HRESULT hr;
  int retry_count = 0;
  do {
    hr = callback.Run();
    if (FAILED(hr))
      base::PlatformThread::Sleep(base::Milliseconds(retry_delay_in_ms_));

    // Give up after some amount of time
  } while (hr == MF_E_INVALIDREQUEST && retry_count++ < max_retry_count_);
  LogNumberOfRetriesNeededToWorkAroundMFInvalidRequest(which_function,
                                                       retry_count);

  return hr;
}

HRESULT VideoCaptureDeviceMFWin::GetDeviceStreamCount(IMFCaptureSource* source,
                                                      DWORD* count) {
  // Sometimes, GetDeviceStreamCount returns an
  // undocumented MF_E_INVALIDREQUEST. Retrying solves the issue.
  return ExecuteHresultCallbackWithRetries(
      base::BindRepeating(
          [](IMFCaptureSource* source, DWORD* count) {
            return source->GetDeviceStreamCount(count);
          },
          base::Unretained(source), count),
      MediaFoundationFunctionRequiringRetry::kGetDeviceStreamCount);
}

HRESULT VideoCaptureDeviceMFWin::GetDeviceStreamCategory(
    IMFCaptureSource* source,
    DWORD stream_index,
    MF_CAPTURE_ENGINE_STREAM_CATEGORY* stream_category) {
  // We believe that GetDeviceStreamCategory could be affected by the same
  // behaviour of GetDeviceStreamCount and GetAvailableDeviceMediaType
  return ExecuteHresultCallbackWithRetries(
      base::BindRepeating(
          [](IMFCaptureSource* source, DWORD stream_index,
             MF_CAPTURE_ENGINE_STREAM_CATEGORY* stream_category) {
            return source->GetDeviceStreamCategory(stream_index,
                                                   stream_category);
          },
          base::Unretained(source), stream_index, stream_category),
      MediaFoundationFunctionRequiringRetry::kGetDeviceStreamCategory);
}

HRESULT VideoCaptureDeviceMFWin::GetAvailableDeviceMediaType(
    IMFCaptureSource* source,
    DWORD stream_index,
    DWORD media_type_index,
    IMFMediaType** type) {
  // Rarely, for some unknown reason, GetAvailableDeviceMediaType returns an
  // undocumented MF_E_INVALIDREQUEST. Retrying solves the issue.
  return ExecuteHresultCallbackWithRetries(
      base::BindRepeating(
          [](IMFCaptureSource* source, DWORD stream_index,
             DWORD media_type_index, IMFMediaType** type) {
            return source->GetAvailableDeviceMediaType(stream_index,
                                                       media_type_index, type);
          },
          base::Unretained(source), stream_index, media_type_index, type),
      MediaFoundationFunctionRequiringRetry::kGetAvailableDeviceMediaType);
}

HRESULT VideoCaptureDeviceMFWin::FillCapabilities(
    IMFCaptureSource* source,
    bool photo,
    CapabilityList* capabilities) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceMFWin::FillCapabilities");
  DWORD stream_count = 0;
  HRESULT hr = GetDeviceStreamCount(source, &stream_count);
  if (FAILED(hr))
    return hr;

  for (DWORD stream_index = 0; stream_index < stream_count; stream_index++) {
    MF_CAPTURE_ENGINE_STREAM_CATEGORY stream_category;
    hr = GetDeviceStreamCategory(source, stream_index, &stream_category);
    if (FAILED(hr))
      return hr;

    if ((photo && stream_category !=
                      MF_CAPTURE_ENGINE_STREAM_CATEGORY_PHOTO_INDEPENDENT) ||
        (!photo &&
         stream_category != MF_CAPTURE_ENGINE_STREAM_CATEGORY_VIDEO_PREVIEW &&
         stream_category != MF_CAPTURE_ENGINE_STREAM_CATEGORY_VIDEO_CAPTURE)) {
      continue;
    }

    DWORD media_type_index = 0;
    ComPtr<IMFMediaType> type;
    while (SUCCEEDED(hr = GetAvailableDeviceMediaType(
                         source, stream_index, media_type_index, &type))) {
      VideoCaptureFormat format;
      VideoPixelFormat source_pixel_format;
      if (GetFormatFromSourceMediaType(
              type.Get(), photo,
              /*use_hardware_format=*/!photo &&
                  static_cast<bool>(dxgi_device_manager_),
              &format, &source_pixel_format)) {
        uint32_t nominal_range = 0;
        auto attribute_hr =
            type->GetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, &nominal_range);
        // 0..255 range NV12 is usually just an unpacked MJPG.
        // Since YUV formats should have 16..235 range, unlike MJPG.
        // Using fake NV12 is discouraged, because if the output is also NV12, a
        // passthrough mode is used by MFCaptureEngine and we get that unusual
        // nominal range in the sink also.
        bool maybe_fake = SUCCEEDED(attribute_hr) &&
                          nominal_range == MFNominalRange_0_255 &&
                          source_pixel_format == media::PIXEL_FORMAT_NV12;
        capabilities->emplace_back(media_type_index, format, stream_index,
                                   source_pixel_format, maybe_fake);
      }

      type.Reset();
      ++media_type_index;
    }
    if (hr == MF_E_NO_MORE_TYPES) {
      hr = S_OK;
    }
    if (FAILED(hr)) {
      return hr;
    }
  }

  return hr;
}

HRESULT VideoCaptureDeviceMFWin::SetAndCommitExtendedCameraControlFlags(
    KSPROPERTY_CAMERACONTROL_EXTENDED_PROPERTY property_id,
    ULONGLONG flags) {
  DCHECK(extended_camera_controller_);
  ComPtr<IMFExtendedCameraControl> extended_camera_control;
  HRESULT hr = extended_camera_controller_->GetExtendedCameraControl(
      MF_CAPTURE_ENGINE_MEDIASOURCE, property_id, &extended_camera_control);
  DLOG_IF_FAILED_WITH_HRESULT("Failed to retrieve IMFExtendedCameraControl",
                              hr);
  if (FAILED(hr)) {
    return hr;
  }
  if (extended_camera_control->GetFlags() != flags) {
    hr = extended_camera_control->SetFlags(flags);
    DLOG_IF_FAILED_WITH_HRESULT("Failed to set extended camera control flags",
                                hr);
    if (FAILED(hr)) {
      return hr;
    }
    hr = extended_camera_control->CommitSettings();
    DLOG_IF_FAILED_WITH_HRESULT(
        "Failed to commit extended camera control settings", hr);
    if (FAILED(hr)) {
      return hr;
    }
  }
  if (camera_control_monitor_) {
    // Save the flags for |OnCameraControlChangeInternal()|.
    set_extended_camera_control_flags_[property_id] = flags;
  }
  return hr;
}

HRESULT VideoCaptureDeviceMFWin::SetCameraControlProperty(
    CameraControlProperty property,
    long value,
    long flags) {
  DCHECK(camera_control_);
  HRESULT hr = camera_control_->Set(property, value, flags);
  if (FAILED(hr)) {
    return hr;
  }
  if (camera_control_monitor_) {
    // Save the value and the flags for |OnCameraControlChangeInternal()|.
    set_camera_control_properties_[property] = {value, flags};
  }
  return hr;
}

HRESULT VideoCaptureDeviceMFWin::SetVideoControlProperty(
    VideoProcAmpProperty property,
    long value,
    long flags) {
  DCHECK(video_control_);
  HRESULT hr = video_control_->Set(property, value, flags);
  if (FAILED(hr)) {
    return hr;
  }
  if (camera_control_monitor_) {
    // Save the value and the flags for |OnCameraControlChangeInternal()|.
    set_video_control_properties_[property] = {value, flags};
  }
  return hr;
}

VideoCaptureDeviceMFWin::VideoCaptureDeviceMFWin(
    const VideoCaptureDeviceDescriptor& device_descriptor,
    ComPtr<IMFMediaSource> source,
    scoped_refptr<DXGIDeviceManager> dxgi_device_manager,
    scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner)
    : VideoCaptureDeviceMFWin(device_descriptor,
                              source,
                              std::move(dxgi_device_manager),
                              nullptr,
                              std::move(main_thread_task_runner)) {}

VideoCaptureDeviceMFWin::VideoCaptureDeviceMFWin(
    const VideoCaptureDeviceDescriptor& device_descriptor,
    ComPtr<IMFMediaSource> source,
    scoped_refptr<DXGIDeviceManager> dxgi_device_manager,
    ComPtr<IMFCaptureEngine> engine,
    scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner)
    : device_descriptor_(device_descriptor),
      create_mf_photo_callback_(base::BindRepeating(&CreateMFPhotoCallback)),
      is_initialized_(false),
      max_retry_count_(200),
      retry_delay_in_ms_(50),
      source_(source),
      engine_(engine),
      is_started_(false),
      has_sent_on_started_to_client_(false),
      exposure_mode_manual_(false),
      focus_mode_manual_(false),
      white_balance_mode_manual_(false),
      capture_initialize_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                          base::WaitableEvent::InitialState::NOT_SIGNALED),
      // We never want to reset |capture_error_|.
      capture_error_(base::WaitableEvent::ResetPolicy::MANUAL,
                     base::WaitableEvent::InitialState::NOT_SIGNALED),
      dxgi_device_manager_(std::move(dxgi_device_manager)),
      main_thread_task_runner_(std::move(main_thread_task_runner)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

VideoCaptureDeviceMFWin::~VideoCaptureDeviceMFWin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DeinitVideoCallbacksControlsAndMonitors();

  // In case there's about to be a new device created with a different config,
  // defer destruction of the IMFCaptureEngine since it force unloads a bunch of
  // DLLs which are expensive to reload.
  if (engine_) {
    main_thread_task_runner_->PostDelayedTask(
        FROM_HERE, base::BindOnce(&DestroyCaptureEngine, std::move(engine_)),
        base::Seconds(5));
  }
}

void VideoCaptureDeviceMFWin::DeinitVideoCallbacksControlsAndMonitors() {
  // Deinitialize (shutdown and reset) video callbacks, control monitors,
  // controls and controllers created by |Init()|.

  camera_control_.Reset();
  video_control_.Reset();
  extended_camera_controller_.Reset();

  if (camera_control_monitor_) {
    camera_control_monitor_->Shutdown();
    camera_control_monitor_.Reset();
  }

  if (video_callback_) {
    video_callback_->Shutdown();
    video_callback_.reset();
  }
}

bool VideoCaptureDeviceMFWin::Init() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceMFWin::Init");
  DCHECK(!is_initialized_);
  HRESULT hr;

  DeinitVideoCallbacksControlsAndMonitors();

  hr = source_.As(&camera_control_);
  DLOG_IF_FAILED_WITH_HRESULT("Failed to retrieve IAMCameraControl", hr);

  hr = source_.As(&video_control_);
  DLOG_IF_FAILED_WITH_HRESULT("Failed to retrieve IAMVideoProcAmp", hr);

  ComPtr<IMFGetService> get_service;
  hr = source_.As(&get_service);
  DLOG_IF_FAILED_WITH_HRESULT("Failed to retrieve IMFGetService", hr);

  if (get_service) {
    hr = get_service->GetService(GUID_NULL,
                                 IID_PPV_ARGS(&extended_camera_controller_));
    DLOG_IF_FAILED_WITH_HRESULT(
        "Failed to retrieve IMFExtendedCameraController", hr);
  }

  if (!engine_) {
    hr = CreateCaptureEngine(&engine_);
    if (FAILED(hr)) {
      LogError(FROM_HERE, hr);
      return false;
    }
  }
  ComPtr<IMFAttributes> attributes;
  hr = MFCreateAttributes(&attributes, 1);
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return false;
  }

  hr = attributes->SetUINT32(MF_CAPTURE_ENGINE_USE_VIDEO_DEVICE_ONLY, TRUE);
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return false;
  }

  if (dxgi_device_manager_) {
    dxgi_device_manager_->RegisterInCaptureEngineAttributes(attributes.Get());
  }

  video_callback_ = new MFVideoCallback(this);
  hr = engine_->Initialize(video_callback_.get(), attributes.Get(), nullptr,
                           source_.Get());
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return false;
  }

  hr = WaitOnCaptureEvent(MF_CAPTURE_ENGINE_INITIALIZED);
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return false;
  }

  CreateMFCameraControlMonitor();

  is_initialized_ = true;
  return true;
}

void VideoCaptureDeviceMFWin::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> client) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceMFWin::AllocateAndStart");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  params_ = params;
  client_ = std::move(client);
  DCHECK_EQ(false, is_started_);

  if (!engine_) {
    OnError(VideoCaptureError::kWinMediaFoundationEngineIsNull, FROM_HERE,
            E_FAIL);
    return;
  }

  ComPtr<IMFCaptureSource> source;
  HRESULT hr = engine_->GetSource(&source);
  if (FAILED(hr)) {
    OnError(VideoCaptureError::kWinMediaFoundationEngineGetSourceFailed,
            FROM_HERE, hr);
    return;
  }

  hr = FillCapabilities(source.Get(), true, &photo_capabilities_);
  if (FAILED(hr)) {
    OnError(VideoCaptureError::kWinMediaFoundationFillPhotoCapabilitiesFailed,
            FROM_HERE, hr);
    return;
  }

  if (!photo_capabilities_.empty()) {
    selected_photo_capability_ =
        std::make_unique<CapabilityWin>(photo_capabilities_.front());
  }

  CapabilityList video_capabilities;
  hr = FillCapabilities(source.Get(), false, &video_capabilities);
  if (FAILED(hr)) {
    OnError(VideoCaptureError::kWinMediaFoundationFillVideoCapabilitiesFailed,
            FROM_HERE, hr);
    return;
  }

  if (video_capabilities.empty()) {
    OnError(VideoCaptureError::kWinMediaFoundationNoVideoCapabilityFound,
            FROM_HERE, "No video capability found");
    return;
  }

  const CapabilityWin best_match_video_capability =
      GetBestMatchedCapability(params.requested_format, video_capabilities);
  ComPtr<IMFMediaType> source_video_media_type;
  hr = GetAvailableDeviceMediaType(
      source.Get(), best_match_video_capability.stream_index,
      best_match_video_capability.media_type_index, &source_video_media_type);
  if (FAILED(hr)) {
    OnError(
        VideoCaptureError::kWinMediaFoundationGetAvailableDeviceMediaTypeFailed,
        FROM_HERE, hr);
    return;
  }

  hr = source->SetCurrentDeviceMediaType(
      best_match_video_capability.stream_index, source_video_media_type.Get());
  if (FAILED(hr)) {
    OnError(
        VideoCaptureError::kWinMediaFoundationSetCurrentDeviceMediaTypeFailed,
        FROM_HERE, hr);
    return;
  }

  ComPtr<IMFCaptureSink> sink;
  hr = engine_->GetSink(MF_CAPTURE_ENGINE_SINK_TYPE_PREVIEW, &sink);
  if (FAILED(hr)) {
    OnError(VideoCaptureError::kWinMediaFoundationEngineGetSinkFailed,
            FROM_HERE, hr);
    return;
  }

  ComPtr<IMFCapturePreviewSink> preview_sink;
  hr = sink->QueryInterface(IID_PPV_ARGS(&preview_sink));
  if (FAILED(hr)) {
    OnError(VideoCaptureError::
                kWinMediaFoundationSinkQueryCapturePreviewInterfaceFailed,
            FROM_HERE, hr);
    return;
  }

  hr = preview_sink->RemoveAllStreams();
  if (FAILED(hr)) {
    OnError(VideoCaptureError::kWinMediaFoundationSinkRemoveAllStreamsFailed,
            FROM_HERE, hr);
    return;
  }

  ComPtr<IMFMediaType> sink_video_media_type;
  hr = MFCreateMediaType(&sink_video_media_type);
  if (FAILED(hr)) {
    OnError(
        VideoCaptureError::kWinMediaFoundationCreateSinkVideoMediaTypeFailed,
        FROM_HERE, hr);
    return;
  }

  hr = ConvertToVideoSinkMediaType(
      source_video_media_type.Get(),
      /*use_hardware_format=*/static_cast<bool>(dxgi_device_manager_),
      sink_video_media_type.Get());
  if (FAILED(hr)) {
    OnError(
        VideoCaptureError::kWinMediaFoundationConvertToVideoSinkMediaTypeFailed,
        FROM_HERE, hr);
    return;
  }

  // If "kWebRTCColorAccuracy" is not enabled,  then nominal range is rewritten
  // to be 16..235 in non-passthrough mode. So update it before extracting the
  // color space information.
  if (!base::FeatureList::IsEnabled(media::kWebRTCColorAccuracy) &&
      (best_match_video_capability.source_pixel_format !=
       best_match_video_capability.supported_format.pixel_format)) {
    source_video_media_type->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE,
                                       MFNominalRange_16_235);
  }
  color_space_ = media::GetMediaTypeColorSpace(source_video_media_type.Get());

  DWORD dw_sink_stream_index = 0;
  hr = preview_sink->AddStream(best_match_video_capability.stream_index,
                               sink_video_media_type.Get(), nullptr,
                               &dw_sink_stream_index);
  if (FAILED(hr)) {
    OnError(VideoCaptureError::kWinMediaFoundationSinkAddStreamFailed,
            FROM_HERE, hr);
    return;
  }

  hr = preview_sink->SetSampleCallback(dw_sink_stream_index,
                                       video_callback_.get());
  if (FAILED(hr)) {
    OnError(VideoCaptureError::kWinMediaFoundationSinkSetSampleCallbackFailed,
            FROM_HERE, hr);
    return;
  }

  // Note, that it is not sufficient to wait for
  // MF_CAPTURE_ENGINE_PREVIEW_STARTED as an indicator that starting capture has
  // succeeded. If the capture device is already in use by a different
  // application, MediaFoundation will still emit
  // MF_CAPTURE_ENGINE_PREVIEW_STARTED, and only after that raise an error
  // event. For the lack of any other events indicating success, we have to wait
  // for the first video frame to arrive before sending our |OnStarted| event to
  // |client_|.
  // We still need to wait for MF_CAPTURE_ENGINE_PREVIEW_STARTED event to ensure
  // that we won't call StopPreview before the preview is started.
  has_sent_on_started_to_client_ = false;
  hr = engine_->StartPreview();
  if (FAILED(hr)) {
    OnError(VideoCaptureError::kWinMediaFoundationEngineStartPreviewFailed,
            FROM_HERE, hr);
    return;
  }

  hr = WaitOnCaptureEvent(MF_CAPTURE_ENGINE_PREVIEW_STARTED);
  if (FAILED(hr)) {
    return;
  }

  selected_video_capability_ =
      std::make_unique<CapabilityWin>(best_match_video_capability);

  is_started_ = true;

  base::UmaHistogramEnumeration(
      "Media.VideoCapture.Win.Device.InternalPixelFormat",
      best_match_video_capability.source_pixel_format,
      media::VideoPixelFormat::PIXEL_FORMAT_MAX);
  base::UmaHistogramEnumeration(
      "Media.VideoCapture.Win.Device.CapturePixelFormat",
      best_match_video_capability.supported_format.pixel_format,
      media::VideoPixelFormat::PIXEL_FORMAT_MAX);
  base::UmaHistogramEnumeration(
      "Media.VideoCapture.Win.Device.RequestedPixelFormat",
      params.requested_format.pixel_format,
      media::VideoPixelFormat::PIXEL_FORMAT_MAX);
}

void VideoCaptureDeviceMFWin::StopAndDeAllocate() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceMFWin::StopAndDeAllocate");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_started_ && engine_) {
    engine_->StopPreview();
  }

  // Ideally, we should wait for MF_CAPTURE_ENGINE_PREVIEW_STOPPED event here.
  // However, since |engine_| is not reused for video capture after here,
  // we can safely ignore this event to reduce the delay.
  // It's only important to ensure that incoming events after the capture has
  // stopped shouldn't lead to any crashes.
  // This is achieved by ensuring that the |video_callback_| is shutdown in the
  // destructor which will stop it from trying to use potentially destroyed
  // VideoCaptureDeviceMFWin instance.
  // Also, the callback itself is ref counted and |engine_| holds the reference,
  // so we can delete this class at any time without creating use-after-free
  // situations.

  is_started_ = false;
  client_.reset();
}

void VideoCaptureDeviceMFWin::TakePhoto(TakePhotoCallback callback) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceMFWin::TakePhoto");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_started_)
    return;

  if (!selected_photo_capability_) {
    video_stream_take_photo_callbacks_.push(std::move(callback));
    return;
  }

  ComPtr<IMFCaptureSource> source;
  HRESULT hr = engine_->GetSource(&source);
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  ComPtr<IMFMediaType> source_media_type;
  hr = GetAvailableDeviceMediaType(
      source.Get(), selected_photo_capability_->stream_index,
      selected_photo_capability_->media_type_index, &source_media_type);
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  hr = source->SetCurrentDeviceMediaType(
      selected_photo_capability_->stream_index, source_media_type.Get());
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  ComPtr<IMFMediaType> sink_media_type;
  hr = MFCreateMediaType(&sink_media_type);
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  hr = ConvertToPhotoSinkMediaType(source_media_type.Get(),
                                   sink_media_type.Get());
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  VideoCaptureFormat format;
  VideoPixelFormat source_format;
  hr = GetFormatFromSourceMediaType(sink_media_type.Get(), true,
                                    /*use_hardware_format=*/false, &format,
                                    &source_format)
           ? S_OK
           : E_FAIL;
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  ComPtr<IMFCaptureSink> sink;
  hr = engine_->GetSink(MF_CAPTURE_ENGINE_SINK_TYPE_PHOTO, &sink);
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  ComPtr<IMFCapturePhotoSink> photo_sink;
  hr = sink->QueryInterface(IID_PPV_ARGS(&photo_sink));
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  hr = photo_sink->RemoveAllStreams();
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  DWORD dw_sink_stream_index = 0;
  hr = photo_sink->AddStream(selected_photo_capability_->stream_index,
                             sink_media_type.Get(), nullptr,
                             &dw_sink_stream_index);
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  scoped_refptr<IMFCaptureEngineOnSampleCallback> photo_callback =
      create_mf_photo_callback_.Run(std::move(callback), format);
  hr = photo_sink->SetSampleCallback(photo_callback.get());
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  hr = engine_->TakePhoto();
  if (FAILED(hr))
    LogError(FROM_HERE, hr);
}

void VideoCaptureDeviceMFWin::GetPhotoState(GetPhotoStateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_started_)
    return;

  ComPtr<IMFCaptureSource> source;
  HRESULT hr = engine_->GetSource(&source);
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  ComPtr<IMFMediaType> current_media_type;
  hr = source->GetCurrentDeviceMediaType(
      selected_photo_capability_ ? selected_photo_capability_->stream_index
                                 : selected_video_capability_->stream_index,
      &current_media_type);
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  auto photo_capabilities = mojo::CreateEmptyPhotoState();
  gfx::Size current_size;
  GetFrameSizeFromMediaType(current_media_type.Get(), &current_size);

  gfx::Size min_size = gfx::Size(current_size.width(), current_size.height());
  gfx::Size max_size = gfx::Size(current_size.width(), current_size.height());
  for (const CapabilityWin& capability : photo_capabilities_) {
    min_size.SetToMin(capability.supported_format.frame_size);
    max_size.SetToMax(capability.supported_format.frame_size);
  }

  photo_capabilities->height = mojom::Range::New(
      max_size.height(), min_size.height(), current_size.height(), 1);
  photo_capabilities->width = mojom::Range::New(
      max_size.width(), min_size.width(), current_size.width(), 1);

  if (camera_control_ && video_control_) {
    photo_capabilities->color_temperature = RetrieveControlRangeAndCurrent(
        video_control_, VideoProcAmp_WhiteBalance,
        &photo_capabilities->supported_white_balance_modes,
        &photo_capabilities->current_white_balance_mode);

    photo_capabilities->exposure_time = RetrieveControlRangeAndCurrent(
        camera_control_, CameraControl_Exposure,
        &photo_capabilities->supported_exposure_modes,
        &photo_capabilities->current_exposure_mode,
        PlatformExposureTimeToCaptureValue, PlatformExposureTimeToCaptureStep);

    photo_capabilities->focus_distance = RetrieveControlRangeAndCurrent(
        camera_control_, CameraControl_Focus,
        &photo_capabilities->supported_focus_modes,
        &photo_capabilities->current_focus_mode);

    photo_capabilities->brightness =
        RetrieveControlRangeAndCurrent(video_control_, VideoProcAmp_Brightness);
    photo_capabilities->contrast =
        RetrieveControlRangeAndCurrent(video_control_, VideoProcAmp_Contrast);
    photo_capabilities->exposure_compensation =
        RetrieveControlRangeAndCurrent(video_control_, VideoProcAmp_Gain);
    // There is no ISO control property in IAMCameraControl or IAMVideoProcAmp
    // interfaces nor any other control property with direct mapping to ISO.
    photo_capabilities->iso = mojom::Range::New();
    photo_capabilities->red_eye_reduction = mojom::RedEyeReduction::NEVER;
    photo_capabilities->saturation =
        RetrieveControlRangeAndCurrent(video_control_, VideoProcAmp_Saturation);
    photo_capabilities->sharpness =
        RetrieveControlRangeAndCurrent(video_control_, VideoProcAmp_Sharpness);
    photo_capabilities->torch = false;
    photo_capabilities->pan = RetrieveControlRangeAndCurrent(
        camera_control_, CameraControl_Pan, nullptr, nullptr,
        PlatformAngleToCaptureValue, PlatformAngleToCaptureStep);
    photo_capabilities->tilt = RetrieveControlRangeAndCurrent(
        camera_control_, CameraControl_Tilt, nullptr, nullptr,
        PlatformAngleToCaptureValue, PlatformAngleToCaptureStep);
    photo_capabilities->zoom =
        RetrieveControlRangeAndCurrent(camera_control_, CameraControl_Zoom);
  }

  if (extended_camera_controller_) {
    ComPtr<IMFExtendedCameraControl> extended_camera_control;
    // KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION is supported in
    // Windows 10 version 20H2. It was updated in Windows 11 version 22H2 to
    // support optional shallow focus capability (according to
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-cameracontrol-extended-backgroundsegmentation)
    // but that support is not needed here.
    hr = extended_camera_controller_->GetExtendedCameraControl(
        MF_CAPTURE_ENGINE_MEDIASOURCE,
        KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION,
        &extended_camera_control);
    DLOG_IF_FAILED_WITH_HRESULT(
        "Failed to retrieve IMFExtendedCameraControl for background "
        "segmentation",
        hr);
    if (SUCCEEDED(hr) && (extended_camera_control->GetCapabilities() &
                          KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR)) {
      photo_capabilities->supported_background_blur_modes = {
          mojom::BackgroundBlurMode::OFF, mojom::BackgroundBlurMode::BLUR};
      photo_capabilities->background_blur_mode =
          (extended_camera_control->GetFlags() &
           KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR)
              ? mojom::BackgroundBlurMode::BLUR
              : mojom::BackgroundBlurMode::OFF;
    }

    hr = extended_camera_controller_->GetExtendedCameraControl(
        MF_CAPTURE_ENGINE_MEDIASOURCE,
        KSPROPERTY_CAMERACONTROL_EXTENDED_DIGITALWINDOW,
        &extended_camera_control);
    DLOG_IF_FAILED_WITH_HRESULT(
        "Failed to retrieve IMFExtendedCameraControl for digital window", hr);
    if (SUCCEEDED(hr) &&
        (extended_camera_control->GetCapabilities() &
         KSCAMERA_EXTENDEDPROP_DIGITALWINDOW_AUTOFACEFRAMING)) {
      photo_capabilities->supported_face_framing_modes = {
          mojom::MeteringMode::NONE, mojom::MeteringMode::CONTINUOUS};
      photo_capabilities->current_face_framing_mode =
          (extended_camera_control->GetFlags() &
           KSCAMERA_EXTENDEDPROP_DIGITALWINDOW_AUTOFACEFRAMING)
              ? mojom::MeteringMode::CONTINUOUS
              : mojom::MeteringMode::NONE;
    }

    hr = extended_camera_controller_->GetExtendedCameraControl(
        MF_CAPTURE_ENGINE_MEDIASOURCE,
        KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION,
        &extended_camera_control);
    DLOG_IF_FAILED_WITH_HRESULT(
        "Failed to retrieve IMFExtendedCameraControl for eye gaze correction",
        hr);
    if (SUCCEEDED(hr)) {
      std::vector<mojom::EyeGazeCorrectionMode> capture_modes =
          ExtendedPlatformFlagsToCaptureModes(
              extended_camera_control->GetCapabilities());
      if (!capture_modes.empty()) {
        photo_capabilities->current_eye_gaze_correction_mode =
            ExtendedPlatformFlagsToCaptureMode(
                extended_camera_control->GetFlags(),
                mojom::EyeGazeCorrectionMode::OFF);
        photo_capabilities->supported_eye_gaze_correction_modes =
            std::move(capture_modes);
      }
    }
  }

  std::move(callback).Run(std::move(photo_capabilities));
}

void VideoCaptureDeviceMFWin::SetPhotoOptions(
    mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_started_)
    return;

  HRESULT hr = S_OK;
  ComPtr<IMFCaptureSource> source;
  hr = engine_->GetSource(&source);

  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  if (!photo_capabilities_.empty() &&
      (settings->has_height || settings->has_width)) {
    ComPtr<IMFMediaType> current_source_media_type;
    hr = source->GetCurrentDeviceMediaType(
        selected_photo_capability_->stream_index, &current_source_media_type);

    if (FAILED(hr)) {
      LogError(FROM_HERE, hr);
      return;
    }

    gfx::Size requested_size = gfx::Size();
    if (settings->has_height)
      requested_size.set_height(settings->height);

    if (settings->has_width)
      requested_size.set_width(settings->width);

    const CapabilityWin best_match = GetBestMatchedPhotoCapability(
        current_source_media_type, requested_size, photo_capabilities_);
    selected_photo_capability_ = std::make_unique<CapabilityWin>(best_match);
  }

  if (camera_control_ && video_control_) {
    if (settings->has_white_balance_mode) {
      if (settings->white_balance_mode == mojom::MeteringMode::CONTINUOUS) {
        hr = SetVideoControlProperty(VideoProcAmp_WhiteBalance, 0L,
                                     VideoProcAmp_Flags_Auto);
        DLOG_IF_FAILED_WITH_HRESULT("Auto white balance config failed", hr);
        if (FAILED(hr))
          return;
        white_balance_mode_manual_ = false;
      } else {
        white_balance_mode_manual_ = true;
      }
    }
    if (white_balance_mode_manual_ && settings->has_color_temperature) {
      hr = SetVideoControlProperty(VideoProcAmp_WhiteBalance,
                                   settings->color_temperature,
                                   VideoProcAmp_Flags_Manual);
      DLOG_IF_FAILED_WITH_HRESULT("Color temperature config failed", hr);
      if (FAILED(hr))
        return;
    }

    if (settings->has_exposure_mode) {
      if (settings->exposure_mode == mojom::MeteringMode::CONTINUOUS) {
        hr = SetCameraControlProperty(CameraControl_Exposure, 0L,
                                      CameraControl_Flags_Auto);
        DLOG_IF_FAILED_WITH_HRESULT("Auto exposure config failed", hr);
        if (FAILED(hr))
          return;
        exposure_mode_manual_ = false;
      } else {
        exposure_mode_manual_ = true;
      }
    }
    if (exposure_mode_manual_ && settings->has_exposure_time) {
      hr = SetCameraControlProperty(
          CameraControl_Exposure,
          CaptureExposureTimeToPlatformValue(settings->exposure_time),
          CameraControl_Flags_Manual);
      DLOG_IF_FAILED_WITH_HRESULT("Exposure Time config failed", hr);
      if (FAILED(hr))
        return;
    }

    if (settings->has_focus_mode) {
      if (settings->focus_mode == mojom::MeteringMode::CONTINUOUS) {
        hr = SetCameraControlProperty(CameraControl_Focus, 0L,
                                      CameraControl_Flags_Auto);
        DLOG_IF_FAILED_WITH_HRESULT("Auto focus config failed", hr);
        if (FAILED(hr))
          return;
        focus_mode_manual_ = false;
      } else {
        focus_mode_manual_ = true;
      }
    }
    if (focus_mode_manual_ && settings->has_focus_distance) {
      hr = SetCameraControlProperty(CameraControl_Focus,
                                    settings->focus_distance,
                                    CameraControl_Flags_Manual);
      DLOG_IF_FAILED_WITH_HRESULT("Focus Distance config failed", hr);
      if (FAILED(hr))
        return;
    }

    if (settings->has_brightness) {
      hr =
          SetVideoControlProperty(VideoProcAmp_Brightness, settings->brightness,
                                  VideoProcAmp_Flags_Manual);
      DLOG_IF_FAILED_WITH_HRESULT("Brightness config failed", hr);
      if (FAILED(hr))
        return;
    }
    if (settings->has_contrast) {
      hr = SetVideoControlProperty(VideoProcAmp_Contrast, settings->contrast,
                                   VideoProcAmp_Flags_Manual);
      DLOG_IF_FAILED_WITH_HRESULT("Contrast config failed", hr);
      if (FAILED(hr))
        return;
    }
    if (settings->has_exposure_compensation) {
      hr = SetVideoControlProperty(VideoProcAmp_Gain,
                                   settings->exposure_compensation,
                                   VideoProcAmp_Flags_Manual);
      DLOG_IF_FAILED_WITH_HRESULT("Exposure Compensation config failed", hr);
      if (FAILED(hr))
        return;
    }
    if (settings->has_saturation) {
      hr =
          SetVideoControlProperty(VideoProcAmp_Saturation, settings->saturation,
                                  VideoProcAmp_Flags_Manual);
      DLOG_IF_FAILED_WITH_HRESULT("Saturation config failed", hr);
      if (FAILED(hr))
        return;
    }
    if (settings->has_sharpness) {
      hr = SetVideoControlProperty(VideoProcAmp_Sharpness, settings->sharpness,
                                   VideoProcAmp_Flags_Manual);
      DLOG_IF_FAILED_WITH_HRESULT("Sharpness config failed", hr);
      if (FAILED(hr))
        return;
    }
    if (settings->has_pan) {
      hr = SetCameraControlProperty(CameraControl_Pan,
                                    CaptureAngleToPlatformValue(settings->pan),
                                    CameraControl_Flags_Manual);
      DLOG_IF_FAILED_WITH_HRESULT("Pan config failed", hr);
      if (FAILED(hr))
        return;
    }
    if (settings->has_tilt) {
      hr = SetCameraControlProperty(CameraControl_Tilt,
                                    CaptureAngleToPlatformValue(settings->tilt),
                                    CameraControl_Flags_Manual);
      DLOG_IF_FAILED_WITH_HRESULT("Tilt config failed", hr);
      if (FAILED(hr))
        return;
    }
    if (settings->has_zoom) {
      hr = SetCameraControlProperty(CameraControl_Zoom, settings->zoom,
                                    CameraControl_Flags_Manual);
      DLOG_IF_FAILED_WITH_HRESULT("Zoom config failed", hr);
      if (FAILED(hr))
        return;
    }
  }

  if (extended_camera_controller_) {
    if (settings->has_background_blur_mode) {
      ULONGLONG flag;
      switch (settings->background_blur_mode) {
        case mojom::BackgroundBlurMode::OFF:
          flag = KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_OFF;
          break;
        case mojom::BackgroundBlurMode::BLUR:
          flag = KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR;
          break;
      }
      hr = SetAndCommitExtendedCameraControlFlags(
          KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION, flag);
      DLOG_IF_FAILED_WITH_HRESULT("Background blur mode config failed", hr);
      if (FAILED(hr)) {
        return;
      }
    }
    if (settings->eye_gaze_correction_mode.has_value()) {
      const ULONGLONG flags = CaptureModeToExtendedPlatformFlags(
          settings->eye_gaze_correction_mode.value());
      hr = SetAndCommitExtendedCameraControlFlags(
          KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION, flags);
      DLOG_IF_FAILED_WITH_HRESULT("Eye gaze correction config failed", hr);
      if (FAILED(hr)) {
        return;
      }
    }
    if (settings->has_face_framing_mode) {
      const ULONGLONG flags =
          settings->face_framing_mode == mojom::MeteringMode::CONTINUOUS
              ? KSCAMERA_EXTENDEDPROP_DIGITALWINDOW_AUTOFACEFRAMING
              : KSCAMERA_EXTENDEDPROP_DIGITALWINDOW_MANUAL;
      hr = SetAndCommitExtendedCameraControlFlags(
          KSPROPERTY_CAMERACONTROL_EXTENDED_DIGITALWINDOW, flags);
      DLOG_IF_FAILED_WITH_HRESULT("Auto face framing config failed", hr);
      if (FAILED(hr)) {
        return;
      }
    }
  }

  std::move(callback).Run(true);
}

void VideoCaptureDeviceMFWin::OnUtilizationReport(
    media::VideoCaptureFeedback feedback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (feedback.require_mapped_frame) {
    last_premapped_request_ = base::TimeTicks::Now();
  }
}

void VideoCaptureDeviceMFWin::OnCameraControlChange(REFGUID control_set,
                                                    UINT32 id) {
  // This is called on IMFCameraControlNotify thread.
  // To serialize all access to this class we post to the task
  // runner which is used for Video capture service API calls
  // (E.g. DeallocateAndStop).
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoCaptureDeviceMFWin::OnCameraControlChangeInternal,
                     weak_factory_.GetWeakPtr(), control_set, id));
}

void VideoCaptureDeviceMFWin::OnCameraControlChangeInternal(REFGUID control_set,
                                                            UINT32 id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ignore changes caused by |SetPhotoOptions()|.
  if (control_set == PROPSETID_VIDCAP_CAMERACONTROL) {
    auto iter = set_camera_control_properties_.find(id);
    if (iter != set_camera_control_properties_.end()) {
      // Get the current value and flags and compare with previously set value
      // and flags. If there are no meaningful differences (the current flags
      // include the previously set auto or manual flag and either the current
      // value equals to the previously set value or the current value is
      // determined automatically), this is not an external configuration
      // change unrelated to |SetPhotoOptions()| of which |client_| should be
      // notified.
      long value, flags;
      if (camera_control_ &&
          SUCCEEDED(camera_control_->Get(id, &value, &flags)) &&
          (flags & (CameraControl_Flags_Auto | CameraControl_Flags_Manual)) ==
              iter->second.flags &&
          (value == iter->second.value || (flags & CameraControl_Flags_Auto))) {
        return;
      }
    }
  } else if (control_set == PROPSETID_VIDCAP_VIDEOPROCAMP) {
    auto iter = set_video_control_properties_.find(id);
    if (iter != set_video_control_properties_.end()) {
      // Get the current value and flags and compare with previously set value
      // and flags. If there are no meaningful differences (the current flags
      // include the previously set auto or manual flag and either the current
      // value equals to the previously set value or the current value is
      // determined automatically), this is not an external configuration
      // change unrelated to |SetPhotoOptions()| of which |client_| should be
      // notified.
      long value, flags;
      if (video_control_ &&
          SUCCEEDED(video_control_->Get(id, &value, &flags)) &&
          (flags & (VideoProcAmp_Flags_Auto | VideoProcAmp_Flags_Manual)) ==
              iter->second.flags &&
          (value == iter->second.value || (flags & VideoProcAmp_Flags_Auto))) {
        return;
      }
    }
  } else if (control_set == KSPROPERTYSETID_ExtendedCameraControl) {
    auto iter = set_extended_camera_control_flags_.find(id);
    if (iter != set_extended_camera_control_flags_.end()) {
      // Get the current flags and compare with previously set flags. If there
      // are no meaningful differences, this is not an external configuration
      // change unrelated to |SetPhotoOptions()| of which |client_| should be
      // notified.
      ComPtr<IMFExtendedCameraControl> extended_camera_control;
      if (extended_camera_controller_ &&
          SUCCEEDED(extended_camera_controller_->GetExtendedCameraControl(
              MF_CAPTURE_ENGINE_MEDIASOURCE, id, &extended_camera_control)) &&
          extended_camera_control->GetFlags() == iter->second) {
        return;
      }
    }
  }

  // Let the client do remaining filtering.
  client_->OnCaptureConfigurationChanged();
}

void VideoCaptureDeviceMFWin::OnCameraControlError(HRESULT status) const {
  // This is called on IMFCameraControlNotify thread.
  LogError(FROM_HERE, status);
}

void VideoCaptureDeviceMFWin::OnIncomingCapturedData(
    Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    base::TimeTicks capture_begin_time) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceMFWin::OnIncomingCapturedData");
  // This is called on IMFCaptureEngine thread.
  // To serialize all access to this class we post to the task
  // runner which is used for Video capture service API calls
  // (E.g. DeallocateAndStop).

  bool need_to_post = false;
  {
    base::AutoLock lock(queueing_lock_);
    need_to_post = !input_buffer_;
    input_buffer_ = std::move(buffer);
    input_reference_time_ = reference_time;
    input_timestamp_ = timestamp;
    input_capture_begin_time_ = capture_begin_time;
  }

  if (need_to_post) {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoCaptureDeviceMFWin::OnIncomingCapturedDataInternal,
                       weak_factory_.GetWeakPtr()));
  }
}

HRESULT VideoCaptureDeviceMFWin::DeliverTextureToClient(
    Microsoft::WRL::ComPtr<IMFMediaBuffer> imf_buffer,
    ID3D11Texture2D* texture,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    base::TimeTicks capture_begin_time) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceMFWin::DeliverTextureToClient");
  // Check for device loss
  Microsoft::WRL::ComPtr<ID3D11Device> texture_device;
  texture->GetDevice(&texture_device);

  HRESULT hr = texture_device->GetDeviceRemovedReason();

  if (FAILED(hr)) {
    // Make sure the main device is reset.
    hr = dxgi_device_manager_->CheckDeviceRemovedAndGetDevice(nullptr);
    LOG(ERROR) << "Camera texture device lost: "
               << logging::SystemErrorCodeToString(hr);
    base::UmaHistogramSparse("Media.VideoCapture.Win.D3DDeviceRemovedReason",
                             hr);
    // Even if device was reset successfully, we can't continue
    // because the texture is tied to the old device.
    return hr;
  }

  if (texture_device.Get() != dxgi_device_manager_->GetDevice().Get()) {
    // Main device has changed while IMFCaptureEngine was producing current
    // texture. Change may happen either due to device removal or due to adapter
    // change signalled via OnGpuInfoUpdate.
    return MF_E_UNEXPECTED;
  }

  gfx::Size texture_size;
  VideoPixelFormat pixel_format;
  bool is_cross_process_shared_texture;
  GetTextureInfo(texture, texture_size, pixel_format,
                 is_cross_process_shared_texture);

  if (pixel_format != PIXEL_FORMAT_NV12) {
    return MF_E_UNSUPPORTED_FORMAT;
  }

  if (base::FeatureList::IsEnabled(kMediaFoundationD3D11VideoCaptureZeroCopy) &&
      is_cross_process_shared_texture) {
    return DeliverExternalBufferToClient(
        std::move(imf_buffer), texture, texture_size, pixel_format,
        reference_time, timestamp, capture_begin_time);
  }

  VideoCaptureDevice::Client::Buffer capture_buffer;
  constexpr int kDummyFrameFeedbackId = 0;
  auto result = client_->ReserveOutputBuffer(
      texture_size, pixel_format, kDummyFrameFeedbackId, &capture_buffer,
      /*require_new_buffer_id=*/nullptr, /*retire_old_buffer_id=*/nullptr);
  if (result != VideoCaptureDevice::Client::ReserveResult::kSucceeded) {
    LOG(ERROR) << "Failed to reserve output capture buffer: " << (int)result;
    return MF_E_UNEXPECTED;
  }

  auto gmb_handle = capture_buffer.handle_provider->GetGpuMemoryBufferHandle();
  if (!gmb_handle.dxgi_handle.IsValid()) {
    // If the device is removed and GMB tracker fails to recreate it,
    // an empty gmb handle may be returned here.
    return MF_E_UNEXPECTED;
  }
  hr = CopyTextureToGpuMemoryBuffer(texture, gmb_handle.dxgi_handle.Get());

  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to copy camera device texture to output texture: "
               << logging::SystemErrorCodeToString(hr);
    return hr;
  }

  capture_buffer.is_premapped = false;
  if (last_premapped_request_ >
      base::TimeTicks::Now() - kMaxFeedbackPremappingEffectDuration) {
    // Only a flag on the Buffer is set here; the region itself isn't passed
    // anywhere because it was passed when the buffer was created.
    // Now the flag would tell the consumer that the region contains actual
    // frame data.
    if (capture_buffer.handle_provider->DuplicateAsUnsafeRegion().IsValid()) {
      capture_buffer.is_premapped = true;
    }
  }

  VideoRotation frame_rotation = VIDEO_ROTATION_0;
  DCHECK(camera_rotation_.has_value());
  switch (camera_rotation_.value()) {
    case 0:
      frame_rotation = VIDEO_ROTATION_0;
      break;
    case 90:
      frame_rotation = VIDEO_ROTATION_90;
      break;
    case 180:
      frame_rotation = VIDEO_ROTATION_180;
      break;
    case 270:
      frame_rotation = VIDEO_ROTATION_270;
      break;
    default:
      break;
  }

  VideoFrameMetadata frame_metadata;
  frame_metadata.transformation = VideoTransformation(frame_rotation);

  client_->OnIncomingCapturedBufferExt(
      std::move(capture_buffer),
      VideoCaptureFormat(
          texture_size, selected_video_capability_->supported_format.frame_rate,
          pixel_format),
      color_space_, reference_time, timestamp,
      MaybeForwardCaptureBeginTime(capture_begin_time), gfx::Rect(texture_size),
      frame_metadata);

  return hr;
}

HRESULT VideoCaptureDeviceMFWin::DeliverExternalBufferToClient(
    Microsoft::WRL::ComPtr<IMFMediaBuffer> imf_buffer,
    ID3D11Texture2D* texture,
    const gfx::Size& texture_size,
    const VideoPixelFormat& pixel_format,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    base::TimeTicks capture_begin_time) {
  UINT private_data_size;
  Microsoft::WRL::ComPtr<DXGIHandlePrivateData> private_data;
  HRESULT hr =
      texture->GetPrivateData(DXGIHandlePrivateDataGUID, &private_data_size,
                              static_cast<void**>(&private_data));

  if (SUCCEEDED(hr) && private_data) {
    DCHECK_EQ(private_data_size, static_cast<UINT>(sizeof(&private_data)));
  } else {
    // It's failed to get valid |private_data|, create and set a new value.
    Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
    hr = texture->QueryInterface(IID_PPV_ARGS(&dxgi_resource));
    if (FAILED(hr)) {
      DLOG(ERROR) << logging::SystemErrorCodeToString(hr);
      return hr;
    }
    HANDLE texture_handle;
    hr = dxgi_resource->CreateSharedHandle(
        nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
        nullptr, &texture_handle);
    if (FAILED(hr)) {
      DLOG(ERROR) << logging::SystemErrorCodeToString(hr);
      return hr;
    }
    private_data = Microsoft::WRL::Make<DXGIHandlePrivateData>(
        base::win::ScopedHandle(texture_handle));

    hr = texture->SetPrivateDataInterface(DXGIHandlePrivateDataGUID,
                                          private_data.Get());
    if (FAILED(hr)) {
      DLOG(ERROR) << "failed to set private data to texture, "
                  << logging::SystemErrorCodeToString(hr);
      return hr;
    }
  }

  // Set reused |token| and |share_handle| to gmb handle.
  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE;
  HANDLE texture_handle_duplicated = nullptr;
  CHECK(::DuplicateHandle(GetCurrentProcess(), private_data->GetTextureHandle(),
                          GetCurrentProcess(), &texture_handle_duplicated, 0,
                          FALSE, DUPLICATE_SAME_ACCESS))
      << "failed to reuse handle.";

  gmb_handle.dxgi_handle.Set(texture_handle_duplicated);
  gmb_handle.dxgi_token = private_data->GetDXGIToken();

  media::CapturedExternalVideoBuffer external_buffer =
      media::CapturedExternalVideoBuffer(
          std::move(imf_buffer), std::move(gmb_handle),
          VideoCaptureFormat(
              texture_size,
              selected_video_capability_->supported_format.frame_rate,
              pixel_format),
          gfx::ColorSpace());
  client_->OnIncomingCapturedExternalBuffer(
      std::move(external_buffer), reference_time, timestamp,
      MaybeForwardCaptureBeginTime(capture_begin_time),
      gfx::Rect(texture_size));
  return hr;
}

void VideoCaptureDeviceMFWin::OnIncomingCapturedDataInternal() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceMFWin::OnIncomingCapturedDataInternal");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
  base::TimeTicks reference_time, capture_begin_time;
  base::TimeDelta timestamp;
  {
    base::AutoLock lock(queueing_lock_);
    buffer = std::move(input_buffer_);
    reference_time = input_reference_time_;
    timestamp = input_timestamp_;
    capture_begin_time = input_capture_begin_time_;
  }

  SendOnStartedIfNotYetSent();

  bool delivered_texture = false;

  if (client_.get()) {
    // We always calculate camera rotation for the first frame. We also cache
    // the latest value to use when AutoRotation is turned off.
    if (!camera_rotation_.has_value() || IsAutoRotationEnabled())
      camera_rotation_ = GetCameraRotation(device_descriptor_.facing);

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    // Use the hardware path only if it is enabled and the produced pixel format
    // is NV12 (which is the only supported one) and the requested format is
    // also NV12.
    if (dxgi_device_manager_ &&
        selected_video_capability_->supported_format.pixel_format ==
            PIXEL_FORMAT_NV12 &&
        params_.requested_format.pixel_format == PIXEL_FORMAT_NV12 &&
        SUCCEEDED(GetTextureFromMFBuffer(buffer.Get(), &texture))) {
      HRESULT hr = DeliverTextureToClient(buffer, texture.Get(), reference_time,
                                          timestamp, capture_begin_time);
      DLOG_IF_FAILED_WITH_HRESULT("Failed to deliver D3D11 texture to client.",
                                  hr);
      delivered_texture = SUCCEEDED(hr);
    }
  }

  if (delivered_texture && video_stream_take_photo_callbacks_.empty()) {
    return;
  }

  ScopedBufferLock locked_buffer(buffer.Get());
  if (!locked_buffer.data()) {
    LOG(ERROR) << "Locked buffer delivered nullptr";
    OnFrameDroppedInternal(
        VideoCaptureFrameDropReason::
            kWinMediaFoundationLockingBufferDelieveredNullptr);
    return;
  }

  if (!delivered_texture && client_.get()) {
    client_->OnIncomingCapturedData(
        locked_buffer.data(), locked_buffer.length(),
        selected_video_capability_->supported_format, color_space_,
        camera_rotation_.value(), false /* flip_y */, reference_time, timestamp,
        MaybeForwardCaptureBeginTime(capture_begin_time));
  }

  while (!video_stream_take_photo_callbacks_.empty()) {
    TakePhotoCallback cb =
        std::move(video_stream_take_photo_callbacks_.front());
    video_stream_take_photo_callbacks_.pop();

    mojom::BlobPtr blob =
        RotateAndBlobify(locked_buffer.data(), locked_buffer.length(),
                         selected_video_capability_->supported_format, 0);
    if (!blob) {
      continue;
    }

    std::move(cb).Run(std::move(blob));
  }
}

void VideoCaptureDeviceMFWin::OnFrameDropped(
    VideoCaptureFrameDropReason reason) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceMFWin::OnFrameDropped");
  // This is called on IMFCaptureEngine thread.
  // To serialize all access to this class we post to the task
  // runner which is used for Video capture service API calls
  // (E.g. DeallocateAndStop).
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoCaptureDeviceMFWin::OnFrameDroppedInternal,
                     weak_factory_.GetWeakPtr(), reason));
}

void VideoCaptureDeviceMFWin::OnFrameDroppedInternal(
    VideoCaptureFrameDropReason reason) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceMFWin::OnFrameDroppedInternal");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SendOnStartedIfNotYetSent();

  if (client_.get()) {
    client_->OnFrameDropped(reason);
  }
}

void VideoCaptureDeviceMFWin::OnEvent(IMFMediaEvent* media_event) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceMFWin::OnEvent");

  HRESULT hr;
  GUID capture_event_guid = GUID_NULL;

  media_event->GetStatus(&hr);
  media_event->GetExtendedType(&capture_event_guid);

  // When MF_CAPTURE_ENGINE_ERROR is returned the captureengine object is no
  // longer valid.
  if (capture_event_guid == MF_CAPTURE_ENGINE_ERROR || FAILED(hr)) {
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                         "VideoCaptureDeviceMFWin::OnEvent",
                         TRACE_EVENT_SCOPE_PROCESS, "error HR", hr);
    // Safe to access this on a potentially different sequence, as
    // this thread is write only and there is a barrier synchronization due
    // to |capture_error_| event.
    last_error_hr_ = hr;
    capture_error_.Signal();
    // There should always be a valid error
    hr = SUCCEEDED(hr) ? E_UNEXPECTED : hr;
    // This is called on IMFCaptureEngine thread.
    // To serialize all access to this class we post to the task
    // runner which is used for Video capture service API calls
    // (E.g. DeallocateAndStop).
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VideoCaptureDeviceMFWin::ProcessEventError,
                                  weak_factory_.GetWeakPtr(), hr));

  } else if (capture_event_guid == MF_CAPTURE_ENGINE_INITIALIZED) {
    capture_initialize_.Signal();
  } else if (capture_event_guid == MF_CAPTURE_ENGINE_PREVIEW_STOPPED) {
    capture_stopped_.Signal();
  } else if (capture_event_guid == MF_CAPTURE_ENGINE_PREVIEW_STARTED) {
    capture_started_.Signal();
  }
}

void VideoCaptureDeviceMFWin::ProcessEventError(HRESULT hr) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceMFWin::ProcessEventError");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (hr == MF_E_HW_MFT_FAILED_START_STREAMING) {
    // This may indicate that the camera is in use by another application.
    if (!activities_report_callback_) {
      activities_report_callback_ = new MFActivitiesReportCallback(
          weak_factory_.GetWeakPtr(), main_thread_task_runner_,
          device_descriptor_.device_id);
    }
    if (!activity_monitor_) {
      bool created = CreateMFSensorActivityMonitor(
          activities_report_callback_.get(), &activity_monitor_);
      if (!created) {
        // Can't rely on activity monitor to check if the camera is in use.
        // Just report the error.
        RecordErrorHistogram(hr);
        OnError(VideoCaptureError::kWinMediaFoundationGetMediaEventStatusFailed,
                FROM_HERE, hr);
        return;
      }
    }
    // Post default action in case there will be no callback calls.
    activity_report_pending_ = true;
    main_thread_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&VideoCaptureDeviceMFWin::OnCameraInUseReport,
                       weak_factory_.GetWeakPtr(), /*in_use=*/false,
                       /*is_default_action=*/true),
        base::Milliseconds(500));
    activity_monitor_->Start();
    return;
  }

  if (hr == DXGI_ERROR_DEVICE_REMOVED && dxgi_device_manager_ != nullptr) {
    // Removed device can happen for external reasons.
    // We should restart capture.
    Microsoft::WRL::ComPtr<ID3D11Device> recreated_d3d_device;
    const bool try_d3d_path = num_restarts_ < kMaxD3DRestarts;
    if (try_d3d_path) {
      HRESULT removed_hr = dxgi_device_manager_->CheckDeviceRemovedAndGetDevice(
          &recreated_d3d_device);
      LOG(ERROR) << "OnEvent: Device was Removed. Reason: "
                 << logging::SystemErrorCodeToString(removed_hr);
    } else {
      // Too many restarts. Fallback to the software path.
      dxgi_device_manager_ = nullptr;
    }

    engine_ = nullptr;
    is_initialized_ = false;
    is_started_ = false;
    source_ = nullptr;
    capture_error_.Reset();
    capture_initialize_.Reset();

    if ((!try_d3d_path || recreated_d3d_device) && RecreateMFSource() &&
        Init()) {
      AllocateAndStart(params_, std::move(client_));
      // If AllocateAndStart fails somehow, OnError() will be called
      // internally. Therefore, it's safe to always override |hr| here.
      hr = S_OK;
      // Ideally we should wait for MF_CAPTURE_ENGINE_PREVIEW_STARTED.
      // However introducing that wait here could deadlocks in case if
      // the same thread is used by MFCaptureEngine to signal events to
      // the client.
      // So we mark |is_started_| speculatevly here.
      is_started_ = true;
      ++num_restarts_;
    } else {
      LOG(ERROR) << "Failed to re-initialize.";
      hr = MF_E_UNEXPECTED;
    }
  }

  if (FAILED(hr)) {
    RecordErrorHistogram(hr);
    OnError(VideoCaptureError::kWinMediaFoundationGetMediaEventStatusFailed,
            FROM_HERE, hr);
  }
}

void VideoCaptureDeviceMFWin::OnError(VideoCaptureError error,
                                      const Location& from_here,
                                      HRESULT hr) {
  OnError(error, from_here, logging::SystemErrorCodeToString(hr).c_str());
}

void VideoCaptureDeviceMFWin::OnError(VideoCaptureError error,
                                      const Location& from_here,
                                      const char* message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!client_.get())
    return;

  client_->OnError(error, from_here,
                   base::StringPrintf("VideoCaptureDeviceMFWin: %s", message));
}

void VideoCaptureDeviceMFWin::SendOnStartedIfNotYetSent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!client_ || has_sent_on_started_to_client_)
    return;
  has_sent_on_started_to_client_ = true;
  client_->OnStarted();
}

HRESULT VideoCaptureDeviceMFWin::WaitOnCaptureEvent(GUID capture_event_guid) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceMFWin::WaitOnCaptureEvent");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  HRESULT hr = S_OK;
  HANDLE events[] = {nullptr, capture_error_.handle()};

  if (capture_event_guid == MF_CAPTURE_ENGINE_INITIALIZED) {
    events[0] = capture_initialize_.handle();
  } else if (capture_event_guid == MF_CAPTURE_ENGINE_PREVIEW_STOPPED) {
    events[0] = capture_stopped_.handle();
  } else if (capture_event_guid == MF_CAPTURE_ENGINE_PREVIEW_STARTED) {
    events[0] = capture_started_.handle();
  } else {
    // no registered event handle for the event requested
    hr = E_NOTIMPL;
    LogError(FROM_HERE, hr);
    return hr;
  }

  DWORD wait_result =
      ::WaitForMultipleObjects(std::size(events), events, FALSE, INFINITE);
  switch (wait_result) {
    case WAIT_OBJECT_0:
      break;
    case WAIT_FAILED:
      hr = HRESULT_FROM_WIN32(::GetLastError());
      LogError(FROM_HERE, hr);
      break;
    default:
      hr = last_error_hr_;
      if (SUCCEEDED(hr)) {
        hr = MF_E_UNEXPECTED;
      }
      LogError(FROM_HERE, hr);
      break;
  }
  return hr;
}

bool VideoCaptureDeviceMFWin::RecreateMFSource() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceMFWin::RecreateMFSource");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const bool is_sensor_api = device_descriptor_.capture_api ==
                             VideoCaptureApi::WIN_MEDIA_FOUNDATION_SENSOR;
  ComPtr<IMFAttributes> attributes;
  HRESULT hr = MFCreateAttributes(&attributes, is_sensor_api ? 3 : 2);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create attributes: "
               << logging::SystemErrorCodeToString(hr);
    return false;
  }
  attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                      MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
  if (is_sensor_api) {
    attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_CATEGORY,
                        KSCATEGORY_SENSOR_CAMERA);
  }
  attributes->SetString(
      MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
      base::SysUTF8ToWide(device_descriptor_.device_id).c_str());
  hr = MFCreateDeviceSource(attributes.Get(), &source_);
  if (FAILED(hr)) {
    LOG(ERROR) << "MFCreateDeviceSource failed: "
               << logging::SystemErrorCodeToString(hr);
    return false;
  }

  if (dxgi_device_manager_) {
    dxgi_device_manager_->RegisterWithMediaSource(source_);
  }
  return true;
}

void VideoCaptureDeviceMFWin::OnCameraInUseReport(bool in_use,
                                                  bool is_default_action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!activity_report_pending_) {
    return;
  }
  activity_report_pending_ = false;

  // Default action for no reports received can be only "camera not in use".
  DCHECK(!in_use || !is_default_action);

  if (in_use) {
    OnError(VideoCaptureError::kWinMediaFoundationCameraBusy, FROM_HERE,
            "Camera is in use by another process");
  } else {
    RecordErrorHistogram(MF_E_HW_MFT_FAILED_START_STREAMING);
    OnError(VideoCaptureError::kWinMediaFoundationGetMediaEventStatusFailed,
            FROM_HERE, MF_E_HW_MFT_FAILED_START_STREAMING);
  }

  if (activity_monitor_) {
    activity_monitor_->Stop();
  }
}

bool CreateMFSensorActivityMonitor(
    IMFSensorActivitiesReportCallback* report_callback,
    IMFSensorActivityMonitor** monitor) {
  // The MF DLLs have been loaded by VideoCaptureDeviceFactoryWin.
  // Just get a DLL module handle here, once.
  static const HMODULE module = GetModuleHandleW(L"mfsensorgroup.dll");
  if (!module) {
    DLOG(ERROR) << "Failed to get the mfsensorgroup.dll module handle";
    return false;
  }

  using MFCreateSensorActivityMonitorType =
      decltype(&MFCreateSensorActivityMonitor);
  static const MFCreateSensorActivityMonitorType create_function =
      reinterpret_cast<MFCreateSensorActivityMonitorType>(
          GetProcAddress(module, "MFCreateSensorActivityMonitor"));
  if (!create_function) {
    DLOG(ERROR) << "Failed to get the MFCreateSensorActivityMonitor function";
    return false;
  }

  HRESULT hr = create_function(report_callback, monitor);
  if (!*monitor) {
    LOG(ERROR) << "Failed to create IMFSensorActivityMonitor: "
               << logging::SystemErrorCodeToString(hr);
    return false;
  }
  return true;
}

}  // namespace media
