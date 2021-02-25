// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/video_capture_device_mf_win.h"

#include <d3d11_4.h>
#include <mfapi.h>
#include <mferror.h>
#include <stddef.h>
#include <wincodec.h>

#include <thread>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/windows_version.h"
#include "media/capture/mojom/image_capture_types.h"
#include "media/capture/video/blob_utils.h"
#include "media/capture/video/win/capability_list_win.h"
#include "media/capture/video/win/sink_filter_win.h"
#include "media/capture/video/win/video_capture_device_utils_win.h"

using base::Location;
using base::win::ScopedCoMem;
using Microsoft::WRL::ComPtr;

namespace media {

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

class MFPhotoCallback final
    : public base::RefCountedThreadSafe<MFPhotoCallback>,
      public IMFCaptureEngineOnSampleCallback {
 public:
  MFPhotoCallback(VideoCaptureDevice::TakePhotoCallback callback,
                  VideoCaptureFormat format)
      : callback_(std::move(callback)), format_(format) {}

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
        LogWindowsImageCaptureOutcome(
            VideoCaptureWinBackend::kMediaFoundation,
            ImageCaptureOutcome::kSucceededUsingPhotoStream,
            IsHighResolution(format_));

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
  ~MFPhotoCallback() {
    if (callback_) {
      LogWindowsImageCaptureOutcome(
          VideoCaptureWinBackend::kMediaFoundation,
          ImageCaptureOutcome::kFailedUsingPhotoStream,
          IsHighResolution(format_));
    }
  }

  VideoCaptureDevice::TakePhotoCallback callback_;
  const VideoCaptureFormat format_;

  DISALLOW_COPY_AND_ASSIGN(MFPhotoCallback);
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
  BYTE* data_ = nullptr;
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
  DPLOG(ERROR) << from_here.ToString()
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

bool GetFormatFromSourceMediaType(IMFMediaType* source_media_type,
                                  bool photo,
                                  bool use_hardware_format,
                                  VideoCaptureFormat* format) {
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
  GUID mf_source_media_subtype;
  GUID mf_sink_media_subtype;
  VideoPixelFormat pixel_format;
};

bool GetMediaFormatConfigurationFromMFSourceMediaSubtype(
    const GUID& mf_source_media_subtype,
    bool use_hardware_format,
    MediaFormatConfiguration* media_format_configuration) {
  // Special case handling of the NV12 format when using hardware capture
  // to ensure that captured buffers are passed through without copies
  if (use_hardware_format && mf_source_media_subtype == MFVideoFormat_NV12) {
    *media_format_configuration = {MFVideoFormat_NV12, MFVideoFormat_NV12,
                                   PIXEL_FORMAT_NV12};
    return true;
  }
  static const MediaFormatConfiguration kMediaFormatConfigurationMap[] = {
      // IMFCaptureEngine inevitably performs the video frame decoding itself.
      // This means that the sink must always be set to an uncompressed video
      // format.

      // Since chromium uses I420 at the other end of the pipe, MF known video
      // output formats are always set to I420.
      {MFVideoFormat_I420, MFVideoFormat_I420, PIXEL_FORMAT_I420},
      {MFVideoFormat_YUY2, MFVideoFormat_I420, PIXEL_FORMAT_I420},
      {MFVideoFormat_UYVY, MFVideoFormat_I420, PIXEL_FORMAT_I420},
      {MFVideoFormat_RGB24, MFVideoFormat_I420, PIXEL_FORMAT_I420},
      {MFVideoFormat_RGB32, MFVideoFormat_I420, PIXEL_FORMAT_I420},
      {MFVideoFormat_ARGB32, MFVideoFormat_I420, PIXEL_FORMAT_I420},
      {MFVideoFormat_MJPG, MFVideoFormat_I420, PIXEL_FORMAT_I420},
      {MFVideoFormat_NV12, MFVideoFormat_I420, PIXEL_FORMAT_I420},
      {MFVideoFormat_YV12, MFVideoFormat_I420, PIXEL_FORMAT_I420},

      // Depth cameras use 16-bit uncompressed video formats.
      // We ask IMFCaptureEngine to let the frame pass through, without
      // transcoding, since transcoding would lead to precision loss.
      {kMediaSubTypeY16, kMediaSubTypeY16, PIXEL_FORMAT_Y16},
      {kMediaSubTypeZ16, kMediaSubTypeZ16, PIXEL_FORMAT_Y16},
      {kMediaSubTypeINVZ, kMediaSubTypeINVZ, PIXEL_FORMAT_Y16},
      {MFVideoFormat_D16, MFVideoFormat_D16, PIXEL_FORMAT_Y16},

      // Photo type
      {GUID_ContainerFormatJpeg, GUID_ContainerFormatJpeg, PIXEL_FORMAT_MJPEG}};

  for (const auto& kMediaFormatConfiguration : kMediaFormatConfigurationMap) {
    if (kMediaFormatConfiguration.mf_source_media_subtype ==
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

void GetTextureSizeAndFormat(ID3D11Texture2D* texture,
                             gfx::Size& size,
                             VideoPixelFormat& format) {
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
}

HRESULT CopyTextureToGpuMemoryBuffer(ID3D11Texture2D* texture,
                                     gfx::GpuMemoryBufferHandle gmb_handle) {
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
  hr = device1->OpenSharedResource1(gmb_handle.dxgi_handle.Get(),
                                    IID_PPV_ARGS(&target_texture));
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

  keyed_mutex->AcquireSync(0, INFINITE);
  device_context->CopySubresourceRegion(target_texture.Get(), 0, 0, 0, 0,
                                        texture, 0, nullptr);
  keyed_mutex->ReleaseSync(0);

  // Need to flush context to ensure that other devices receive updated contents
  // of shared resource
  device_context->Flush();

  return S_OK;
}

}  // namespace

class MFVideoCallback final
    : public base::RefCountedThreadSafe<MFVideoCallback>,
      public IMFCaptureEngineOnSampleCallback,
      public IMFCaptureEngineOnEventCallback {
 public:
  MFVideoCallback(VideoCaptureDeviceMFWin* observer) : observer_(observer) {}

  IFACEMETHODIMP QueryInterface(REFIID riid, void** object) override {
    HRESULT hr = E_NOINTERFACE;
    if (riid == IID_IUnknown) {
      *object = this;
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

  IFACEMETHODIMP OnEvent(IMFMediaEvent* media_event) override {
    base::AutoLock lock(lock_);
    if (!observer_) {
      return S_OK;
    }
    observer_->OnEvent(media_event);
    return S_OK;
  }

  IFACEMETHODIMP OnSample(IMFSample* sample) override {
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
    LONGLONG raw_time_stamp = 0;
    sample->GetSampleTime(&raw_time_stamp);
    base::TimeDelta timestamp =
        base::TimeDelta::FromMicroseconds(raw_time_stamp / 10);

    DWORD count = 0;
    sample->GetBufferCount(&count);

    for (DWORD i = 0; i < count; ++i) {
      ComPtr<IMFMediaBuffer> buffer;
      sample->GetBufferByIndex(i, &buffer);
      if (buffer) {
        observer_->OnIncomingCapturedData(buffer.Get(), reference_time,
                                          timestamp);
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

  // Protects access to |observer_|.
  base::Lock lock_;
  VideoCaptureDeviceMFWin* observer_ GUARDED_BY(lock_);
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
  HRESULT hr = source.As(&camera_control);
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
      base::PlatformThread::Sleep(
          base::TimeDelta::FromMilliseconds(retry_delay_in_ms_));

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
      if (GetFormatFromSourceMediaType(
              type.Get(), photo,
              /*use_hardware_format=*/!photo &&
                  static_cast<bool>(dxgi_device_manager_),
              &format))
        capabilities->emplace_back(media_type_index, format, stream_index);
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

VideoCaptureDeviceMFWin::VideoCaptureDeviceMFWin(
    const VideoCaptureDeviceDescriptor& device_descriptor,
    ComPtr<IMFMediaSource> source,
    scoped_refptr<DXGIDeviceManager> dxgi_device_manager)
    : VideoCaptureDeviceMFWin(device_descriptor,
                              source,
                              std::move(dxgi_device_manager),
                              nullptr) {}

VideoCaptureDeviceMFWin::VideoCaptureDeviceMFWin(
    const VideoCaptureDeviceDescriptor& device_descriptor,
    ComPtr<IMFMediaSource> source,
    scoped_refptr<DXGIDeviceManager> dxgi_device_manager,
    ComPtr<IMFCaptureEngine> engine)
    : facing_mode_(device_descriptor.facing),
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
      dxgi_device_manager_(std::move(dxgi_device_manager)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

VideoCaptureDeviceMFWin::~VideoCaptureDeviceMFWin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!video_stream_take_photo_callbacks_.empty()) {
    for (size_t k = 0; k < video_stream_take_photo_callbacks_.size(); k++) {
      LogWindowsImageCaptureOutcome(
          VideoCaptureWinBackend::kMediaFoundation,
          ImageCaptureOutcome::kFailedUsingVideoStream,
          selected_video_capability_
              ? IsHighResolution(selected_video_capability_->supported_format)
              : false);
    }
  }
  if (video_callback_) {
    video_callback_->Shutdown();
  }
}

bool VideoCaptureDeviceMFWin::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_initialized_);
  HRESULT hr;

  hr = source_.As(&camera_control_);
  DLOG_IF_FAILED_WITH_HRESULT("Failed to retrieve IAMCameraControl", hr);

  hr = source_.As(&video_control_);
  DLOG_IF_FAILED_WITH_HRESULT("Failed to retrieve IAMVideoProcAmp", hr);

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

  is_initialized_ = true;
  return true;
}

void VideoCaptureDeviceMFWin::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock lock(lock_);

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
    selected_photo_capability_.reset(
        new CapabilityWin(photo_capabilities_.front()));
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
  has_sent_on_started_to_client_ = false;
  hr = engine_->StartPreview();
  if (FAILED(hr)) {
    OnError(VideoCaptureError::kWinMediaFoundationEngineStartPreviewFailed,
            FROM_HERE, hr);
    return;
  }

  selected_video_capability_.reset(
      new CapabilityWin(best_match_video_capability));

  is_started_ = true;
}

void VideoCaptureDeviceMFWin::StopAndDeAllocate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock lock(lock_);

  if (is_started_ && engine_)
    engine_->StopPreview();
  is_started_ = false;

  client_.reset();
}

void VideoCaptureDeviceMFWin::TakePhoto(TakePhotoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock lock(lock_);

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
  hr = GetFormatFromSourceMediaType(sink_media_type.Get(), true,
                                    /*use_hardware_format=*/false, &format)
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
    if (FAILED(hr)) {
      LogError(FROM_HERE, hr);
      return;
    }

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
    selected_photo_capability_.reset(new CapabilityWin(best_match));
  }

  if (camera_control_ && video_control_) {
    if (settings->has_white_balance_mode) {
      if (settings->white_balance_mode == mojom::MeteringMode::CONTINUOUS) {
        hr = video_control_->Set(VideoProcAmp_WhiteBalance, 0L,
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
      hr = video_control_->Set(VideoProcAmp_WhiteBalance,
                               settings->color_temperature,
                               VideoProcAmp_Flags_Manual);
      DLOG_IF_FAILED_WITH_HRESULT("Color temperature config failed", hr);
      if (FAILED(hr))
        return;
    }

    if (settings->has_exposure_mode) {
      if (settings->exposure_mode == mojom::MeteringMode::CONTINUOUS) {
        hr = camera_control_->Set(CameraControl_Exposure, 0L,
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
      hr = camera_control_->Set(
          CameraControl_Exposure,
          CaptureExposureTimeToPlatformValue(settings->exposure_time),
          CameraControl_Flags_Manual);
      DLOG_IF_FAILED_WITH_HRESULT("Exposure Time config failed", hr);
      if (FAILED(hr))
        return;
    }

    if (settings->has_focus_mode) {
      if (settings->focus_mode == mojom::MeteringMode::CONTINUOUS) {
        hr = camera_control_->Set(CameraControl_Focus, 0L,
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
      hr = camera_control_->Set(CameraControl_Focus, settings->focus_distance,
                                CameraControl_Flags_Manual);
      DLOG_IF_FAILED_WITH_HRESULT("Focus Distance config failed", hr);
      if (FAILED(hr))
        return;
    }

    if (settings->has_brightness) {
      hr = video_control_->Set(VideoProcAmp_Brightness, settings->brightness,
                               VideoProcAmp_Flags_Manual);
      DLOG_IF_FAILED_WITH_HRESULT("Brightness config failed", hr);
      if (FAILED(hr))
        return;
    }
    if (settings->has_contrast) {
      hr = video_control_->Set(VideoProcAmp_Contrast, settings->contrast,
                               VideoProcAmp_Flags_Manual);
      DLOG_IF_FAILED_WITH_HRESULT("Contrast config failed", hr);
      if (FAILED(hr))
        return;
    }
    if (settings->has_exposure_compensation) {
      hr = video_control_->Set(VideoProcAmp_Gain,
                               settings->exposure_compensation,
                               VideoProcAmp_Flags_Manual);
      DLOG_IF_FAILED_WITH_HRESULT("Exposure Compensation config failed", hr);
      if (FAILED(hr))
        return;
    }
    if (settings->has_saturation) {
      hr = video_control_->Set(VideoProcAmp_Saturation, settings->saturation,
                               VideoProcAmp_Flags_Manual);
      DLOG_IF_FAILED_WITH_HRESULT("Saturation config failed", hr);
      if (FAILED(hr))
        return;
    }
    if (settings->has_sharpness) {
      hr = video_control_->Set(VideoProcAmp_Sharpness, settings->sharpness,
                               VideoProcAmp_Flags_Manual);
      DLOG_IF_FAILED_WITH_HRESULT("Sharpness config failed", hr);
      if (FAILED(hr))
        return;
    }
    if (settings->has_pan) {
      hr = camera_control_->Set(CameraControl_Pan,
                                CaptureAngleToPlatformValue(settings->pan),
                                CameraControl_Flags_Manual);
      DLOG_IF_FAILED_WITH_HRESULT("Pan config failed", hr);
      if (FAILED(hr))
        return;
    }
    if (settings->has_tilt) {
      hr = camera_control_->Set(CameraControl_Tilt,
                                CaptureAngleToPlatformValue(settings->tilt),
                                CameraControl_Flags_Manual);
      DLOG_IF_FAILED_WITH_HRESULT("Tilt config failed", hr);
      if (FAILED(hr))
        return;
    }
    if (settings->has_zoom) {
      hr = camera_control_->Set(CameraControl_Zoom, settings->zoom,
                                CameraControl_Flags_Manual);
      DLOG_IF_FAILED_WITH_HRESULT("Zoom config failed", hr);
      if (FAILED(hr))
        return;
    }
  }

  std::move(callback).Run(true);
}
void VideoCaptureDeviceMFWin::OnIncomingCapturedData(
    IMFMediaBuffer* buffer,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp) {
  VideoCaptureFrameDropReason frame_drop_reason =
      VideoCaptureFrameDropReason::kNone;
  OnIncomingCapturedDataInternal(buffer, reference_time, timestamp,
                                 frame_drop_reason);
  if (frame_drop_reason != VideoCaptureFrameDropReason::kNone) {
    OnFrameDropped(frame_drop_reason);
  }
}

HRESULT VideoCaptureDeviceMFWin::DeliverTextureToClient(
    ID3D11Texture2D* texture,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp) {
  // Check for device loss
  Microsoft::WRL::ComPtr<ID3D11Device> texture_device;
  texture->GetDevice(&texture_device);

  HRESULT hr = texture_device->GetDeviceRemovedReason();

  if (FAILED(hr)) {
    DLOG(ERROR) << "Camera texture device lost.";
    DCHECK(dxgi_device_manager_->ResetDevice());
    return hr;
  }

  gfx::Size texture_size;
  VideoPixelFormat pixel_format;
  GetTextureSizeAndFormat(texture, texture_size, pixel_format);

  if (pixel_format != PIXEL_FORMAT_NV12) {
    return MF_E_UNSUPPORTED_FORMAT;
  }

  VideoCaptureDevice::Client::Buffer capture_buffer;
  constexpr int kDummyFrameFeedbackId = 0;
  auto result = client_->ReserveOutputBuffer(
      texture_size, pixel_format, kDummyFrameFeedbackId, &capture_buffer);
  if (result != VideoCaptureDevice::Client::ReserveResult::kSucceeded) {
    DLOG(ERROR) << "Failed to reserve output capture buffer: " << (int)result;
    return MF_E_UNEXPECTED;
  }

  hr = CopyTextureToGpuMemoryBuffer(
      texture, capture_buffer.handle_provider->GetGpuMemoryBufferHandle());

  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to copy camera device texture to output texture: "
                << logging::SystemErrorCodeToString(hr);
    return hr;
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
      gfx::ColorSpace(), reference_time, timestamp, gfx::Rect(texture_size),
      frame_metadata);

  return hr;
}

void VideoCaptureDeviceMFWin::OnIncomingCapturedDataInternal(
    IMFMediaBuffer* buffer,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    VideoCaptureFrameDropReason& frame_drop_reason) {
  base::AutoLock lock(lock_);

  SendOnStartedIfNotYetSent();

  bool delivered_texture = false;

  if (client_.get()) {
    if (!has_sent_on_started_to_client_) {
      has_sent_on_started_to_client_ = true;
      client_->OnStarted();
    }

    // We always calculate camera rotation for the first frame. We also cache
    // the latest value to use when AutoRotation is turned off.
    if (!camera_rotation_.has_value() || IsAutoRotationEnabled())
      camera_rotation_ = GetCameraRotation(facing_mode_);

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    if (dxgi_device_manager_ &&
        SUCCEEDED(GetTextureFromMFBuffer(buffer, &texture))) {
      HRESULT hr =
          DeliverTextureToClient(texture.Get(), reference_time, timestamp);
      DLOG_IF_FAILED_WITH_HRESULT("Failed to deliver D3D11 texture to client.",
                                  hr);
      delivered_texture = SUCCEEDED(hr);
    }
  }

  if (delivered_texture && video_stream_take_photo_callbacks_.empty()) {
    return;
  }

  ScopedBufferLock locked_buffer(buffer);
  if (!locked_buffer.data()) {
    DLOG(ERROR) << "Locked buffer delivered nullptr";
    frame_drop_reason = VideoCaptureFrameDropReason::
        kWinMediaFoundationLockingBufferDelieveredNullptr;
    return;
  }

  if (!delivered_texture && client_.get()) {
    // TODO(julien.isorce): retrieve the color space information using Media
    // Foundation api, MFGetAttributeSize/MF_MT_VIDEO_PRIMARIES,in order to
    // build a gfx::ColorSpace. See http://crbug.com/959988.
    client_->OnIncomingCapturedData(
        locked_buffer.data(), locked_buffer.length(),
        selected_video_capability_->supported_format, gfx::ColorSpace(),
        camera_rotation_.value(), false /* flip_y */, reference_time,
        timestamp);
  }

  while (!video_stream_take_photo_callbacks_.empty()) {
    TakePhotoCallback cb =
        std::move(video_stream_take_photo_callbacks_.front());
    video_stream_take_photo_callbacks_.pop();

    mojom::BlobPtr blob =
        RotateAndBlobify(locked_buffer.data(), locked_buffer.length(),
                         selected_video_capability_->supported_format, 0);
    if (!blob) {
      LogWindowsImageCaptureOutcome(
          VideoCaptureWinBackend::kMediaFoundation,
          ImageCaptureOutcome::kFailedUsingVideoStream,
          IsHighResolution(selected_video_capability_->supported_format));
      continue;
    }

    std::move(cb).Run(std::move(blob));
    LogWindowsImageCaptureOutcome(
        VideoCaptureWinBackend::kMediaFoundation,
        ImageCaptureOutcome::kSucceededUsingVideoStream,
        IsHighResolution(selected_video_capability_->supported_format));
  }
}

void VideoCaptureDeviceMFWin::OnFrameDropped(
    VideoCaptureFrameDropReason reason) {
  base::AutoLock lock(lock_);

  SendOnStartedIfNotYetSent();

  if (client_.get()) {
    client_->OnFrameDropped(reason);
  }
}

void VideoCaptureDeviceMFWin::OnEvent(IMFMediaEvent* media_event) {
  base::AutoLock lock(lock_);

  HRESULT hr;
  GUID capture_event_guid = GUID_NULL;

  media_event->GetStatus(&hr);
  media_event->GetExtendedType(&capture_event_guid);
  // TODO(http://crbug.com/1093521): Add cases for Start
  // MF_CAPTURE_ENGINE_PREVIEW_STARTED and MF_CAPTURE_ENGINE_PREVIEW_STOPPED
  // When MF_CAPTURE_ENGINE_ERROR is returned the captureengine object is no
  // longer valid.
  if (capture_event_guid == MF_CAPTURE_ENGINE_ERROR || FAILED(hr)) {
    capture_error_.Signal();
    // There should always be a valid error
    hr = SUCCEEDED(hr) ? E_UNEXPECTED : hr;
  } else if (capture_event_guid == MF_CAPTURE_ENGINE_INITIALIZED) {
    capture_initialize_.Signal();
  }

  if (FAILED(hr))
    OnError(VideoCaptureError::kWinMediaFoundationGetMediaEventStatusFailed,
            FROM_HERE, hr);
}

void VideoCaptureDeviceMFWin::OnError(VideoCaptureError error,
                                      const Location& from_here,
                                      HRESULT hr) {
  OnError(error, from_here, logging::SystemErrorCodeToString(hr).c_str());
}

void VideoCaptureDeviceMFWin::OnError(VideoCaptureError error,
                                      const Location& from_here,
                                      const char* message) {
  if (!client_.get())
    return;

  client_->OnError(error, from_here,
                   base::StringPrintf("VideoCaptureDeviceMFWin: %s", message));
}

void VideoCaptureDeviceMFWin::SendOnStartedIfNotYetSent() {
  if (!client_ || has_sent_on_started_to_client_)
    return;
  has_sent_on_started_to_client_ = true;
  client_->OnStarted();
}

HRESULT VideoCaptureDeviceMFWin::WaitOnCaptureEvent(GUID capture_event_guid) {
  HRESULT hr = S_OK;
  HANDLE events[] = {nullptr, capture_error_.handle()};

  // TODO(http://crbug.com/1093521): Add cases for Start
  // MF_CAPTURE_ENGINE_PREVIEW_STARTED and MF_CAPTURE_ENGINE_PREVIEW_STOPPED
  if (capture_event_guid == MF_CAPTURE_ENGINE_INITIALIZED) {
    events[0] = capture_initialize_.handle();
  } else {
    // no registered event handle for the event requested
    hr = E_NOTIMPL;
    LogError(FROM_HERE, hr);
    return hr;
  }

  DWORD wait_result =
      ::WaitForMultipleObjects(base::size(events), events, FALSE, INFINITE);
  switch (wait_result) {
    case WAIT_OBJECT_0:
      break;
    case WAIT_FAILED:
      hr = HRESULT_FROM_WIN32(::GetLastError());
      LogError(FROM_HERE, hr);
      break;
    default:
      hr = E_UNEXPECTED;
      LogError(FROM_HERE, hr);
      break;
  }
  return hr;
}
}  // namespace media
