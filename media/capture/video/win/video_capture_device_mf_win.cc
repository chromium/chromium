// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/video_capture_device_mf_win.h"

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

namespace {

class MFPhotoCallback final
    : public base::RefCountedThreadSafe<MFPhotoCallback>,
      public IMFCaptureEngineOnSampleCallback {
 public:
  MFPhotoCallback(VideoCaptureDevice::TakePhotoCallback callback,
                  VideoCaptureFormat format)
      : callback_(std::move(callback)), format_(format) {}

  STDMETHOD(QueryInterface)(REFIID riid, void** object) override {
    if (riid == IID_IUnknown || riid == IID_IMFCaptureEngineOnSampleCallback) {
      AddRef();
      *object = static_cast<IMFCaptureEngineOnSampleCallback*>(this);
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  STDMETHOD_(ULONG, AddRef)() override {
    base::RefCountedThreadSafe<MFPhotoCallback>::AddRef();
    return 1U;
  }

  STDMETHOD_(ULONG, Release)() override {
    base::RefCountedThreadSafe<MFPhotoCallback>::Release();
    return 1U;
  }

  STDMETHOD(OnSample)(IMFSample* sample) override {
    if (!sample)
      return S_OK;

    DWORD buffer_count = 0;
    sample->GetBufferCount(&buffer_count);

    for (DWORD i = 0; i < buffer_count; ++i) {
      ComPtr<IMFMediaBuffer> buffer;
      sample->GetBufferByIndex(i, buffer.GetAddressOf());
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
          sub_type_guid, &format->pixel_format)) {
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
    MediaFormatConfiguration* media_format_configuration) {
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
                              GUID* mf_sink_media_subtype,
                              bool* passthrough) {
  GUID source_subtype;
  HRESULT hr = source_media_type->GetGUID(MF_MT_SUBTYPE, &source_subtype);
  if (FAILED(hr))
    return hr;
  MediaFormatConfiguration media_format_configuration;
  if (!GetMediaFormatConfigurationFromMFSourceMediaSubtype(
          source_subtype, &media_format_configuration))
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
  hr = GetMFSinkMediaSubtype(source_media_type, &mf_sink_media_subtype,
                             &passthrough);
  if (FAILED(hr))
    return hr;

  hr = destination_media_type->SetGUID(MF_MT_SUBTYPE, mf_sink_media_subtype);
  if (FAILED(hr))
    return hr;

  return CopyAttribute(source_media_type, destination_media_type,
                       MF_MT_FRAME_SIZE);
}

HRESULT ConvertToVideoSinkMediaType(IMFMediaType* source_media_type,
                                    IMFMediaType* sink_media_type) {
  HRESULT hr = sink_media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  if (FAILED(hr))
    return hr;

  bool passthrough = false;
  GUID mf_sink_media_subtype;
  hr = GetMFSinkMediaSubtype(source_media_type, &mf_sink_media_subtype,
                             &passthrough);
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
  HRESULT hr = CoCreateInstance(
      CLSID_MFCaptureEngineClassFactory, NULL, CLSCTX_INPROC_SERVER,
      IID_PPV_ARGS(capture_engine_class_factory.GetAddressOf()));
  if (FAILED(hr))
    return hr;

  return capture_engine_class_factory->CreateInstance(CLSID_MFCaptureEngine,
                                                      IID_PPV_ARGS(engine));
}
}  // namespace

class MFVideoCallback final
    : public base::RefCountedThreadSafe<MFVideoCallback>,
      public IMFCaptureEngineOnSampleCallback,
      public IMFCaptureEngineOnEventCallback {
 public:
  MFVideoCallback(VideoCaptureDeviceMFWin* observer) : observer_(observer) {}

  STDMETHOD(QueryInterface)(REFIID riid, void** object) override {
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

  STDMETHOD_(ULONG, AddRef)() override {
    base::RefCountedThreadSafe<MFVideoCallback>::AddRef();
    return 1U;
  }

  STDMETHOD_(ULONG, Release)() override {
    base::RefCountedThreadSafe<MFVideoCallback>::Release();
    return 1U;
  }

  STDMETHOD(OnEvent)(IMFMediaEvent* media_event) override {
    observer_->OnEvent(media_event);
    return S_OK;
  }

  STDMETHOD(OnSample)(IMFSample* sample) override {
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
      sample->GetBufferByIndex(i, buffer.GetAddressOf());
      if (buffer) {
        DWORD length = 0, max_length = 0;
        BYTE* data = NULL;
        buffer->Lock(&data, &max_length, &length);
        if (data) {
          observer_->OnIncomingCapturedData(data, length, reference_time,
                                            timestamp);
        } else {
          observer_->OnFrameDropped(
              VideoCaptureFrameDropReason::
                  kWinMediaFoundationLockingBufferDelieveredNullptr);
        }
        buffer->Unlock();
      } else {
        observer_->OnFrameDropped(
            VideoCaptureFrameDropReason::
                kWinMediaFoundationGetBufferByIndexReturnedNull);
      }
    }
    return S_OK;
  }

 private:
  friend class base::RefCountedThreadSafe<MFVideoCallback>;
  ~MFVideoCallback() {}
  VideoCaptureDeviceMFWin* observer_;
};

// static
bool VideoCaptureDeviceMFWin::GetPixelFormatFromMFSourceMediaSubtype(
    const GUID& mf_source_media_subtype,
    VideoPixelFormat* pixel_format) {
  MediaFormatConfiguration media_format_configuration;
  if (!GetMediaFormatConfigurationFromMFSourceMediaSubtype(
          mf_source_media_subtype, &media_format_configuration))
    return false;

  *pixel_format = media_format_configuration.pixel_format;
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
    while (SUCCEEDED(hr = GetAvailableDeviceMediaType(source, stream_index,
                                                      media_type_index,
                                                      type.GetAddressOf()))) {
      VideoCaptureFormat format;
      if (GetFormatFromSourceMediaType(type.Get(), photo, &format))
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
    ComPtr<IMFMediaSource> source)
    : VideoCaptureDeviceMFWin(device_descriptor, source, nullptr) {}

VideoCaptureDeviceMFWin::VideoCaptureDeviceMFWin(
    const VideoCaptureDeviceDescriptor& device_descriptor,
    ComPtr<IMFMediaSource> source,
    ComPtr<IMFCaptureEngine> engine)
    : facing_mode_(device_descriptor.facing),
      create_mf_photo_callback_(base::BindRepeating(&CreateMFPhotoCallback)),
      is_initialized_(false),
      max_retry_count_(200),
      retry_delay_in_ms_(50),
      source_(source),
      engine_(engine),
      is_started_(false),
      has_sent_on_started_to_client_(false) {
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
}

bool VideoCaptureDeviceMFWin::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_initialized_);

  HRESULT hr = S_OK;
  if (!engine_)
    hr = CreateCaptureEngine(engine_.GetAddressOf());

  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return false;
  }

  ComPtr<IMFAttributes> attributes;
  MFCreateAttributes(attributes.GetAddressOf(), 1);
  DCHECK(attributes);

  video_callback_ = new MFVideoCallback(this);
  hr = engine_->Initialize(video_callback_.get(), attributes.Get(), nullptr,
                           source_.Get());
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
  HRESULT hr = engine_->GetSource(source.GetAddressOf());
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
  hr = GetAvailableDeviceMediaType(source.Get(),
                                   best_match_video_capability.stream_index,
                                   best_match_video_capability.media_type_index,
                                   source_video_media_type.GetAddressOf());
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
  hr = engine_->GetSink(MF_CAPTURE_ENGINE_SINK_TYPE_PREVIEW,
                        sink.GetAddressOf());
  if (FAILED(hr)) {
    OnError(VideoCaptureError::kWinMediaFoundationEngineGetSinkFailed,
            FROM_HERE, hr);
    return;
  }

  ComPtr<IMFCapturePreviewSink> preview_sink;
  hr = sink->QueryInterface(IID_PPV_ARGS(preview_sink.GetAddressOf()));
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
  hr = MFCreateMediaType(sink_video_media_type.GetAddressOf());
  if (FAILED(hr)) {
    OnError(
        VideoCaptureError::kWinMediaFoundationCreateSinkVideoMediaTypeFailed,
        FROM_HERE, hr);
    return;
  }

  hr = ConvertToVideoSinkMediaType(source_video_media_type.Get(),
                                   sink_video_media_type.Get());
  if (FAILED(hr)) {
    OnError(
        VideoCaptureError::kWinMediaFoundationConvertToVideoSinkMediaTypeFailed,
        FROM_HERE, hr);
    return;
  }

  DWORD dw_sink_stream_index = 0;
  hr = preview_sink->AddStream(best_match_video_capability.stream_index,
                               sink_video_media_type.Get(), NULL,
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
  HRESULT hr = engine_->GetSource(source.GetAddressOf());
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  ComPtr<IMFMediaType> source_media_type;
  hr = GetAvailableDeviceMediaType(source.Get(),
                                   selected_photo_capability_->stream_index,
                                   selected_photo_capability_->media_type_index,
                                   source_media_type.GetAddressOf());
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
  hr = MFCreateMediaType(sink_media_type.GetAddressOf());
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
  hr = GetFormatFromSourceMediaType(sink_media_type.Get(), true, &format)
           ? S_OK
           : E_FAIL;
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  ComPtr<IMFCaptureSink> sink;
  hr = engine_->GetSink(MF_CAPTURE_ENGINE_SINK_TYPE_PHOTO, sink.GetAddressOf());
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  ComPtr<IMFCapturePhotoSink> photo_sink;
  hr = sink->QueryInterface(IID_PPV_ARGS(photo_sink.GetAddressOf()));
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
  hr =
      photo_sink->AddStream(selected_photo_capability_->stream_index,
                            sink_media_type.Get(), NULL, &dw_sink_stream_index);
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
  HRESULT hr = engine_->GetSource(source.GetAddressOf());
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  ComPtr<IMFMediaType> current_media_type;
  hr = source->GetCurrentDeviceMediaType(
      selected_photo_capability_ ? selected_photo_capability_->stream_index
                                 : selected_video_capability_->stream_index,
      current_media_type.GetAddressOf());
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
  hr = engine_->GetSource(source.GetAddressOf());

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
        selected_photo_capability_->stream_index,
        current_source_media_type.GetAddressOf());

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

  std::move(callback).Run(true);
}

void VideoCaptureDeviceMFWin::OnIncomingCapturedData(
    const uint8_t* data,
    int length,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp) {
  base::AutoLock lock(lock_);
  DCHECK(data);

  SendOnStartedIfNotYetSent();

  if (client_.get()) {
    if (!has_sent_on_started_to_client_) {
      has_sent_on_started_to_client_ = true;
      client_->OnStarted();
    }

    // TODO(julien.isorce): retrieve the color space information using Media
    // Foundation api, MFGetAttributeSize/MF_MT_VIDEO_PRIMARIES,in order to
    // build a gfx::ColorSpace. See http://crbug.com/959988.
    client_->OnIncomingCapturedData(
        data, length, selected_video_capability_->supported_format,
        gfx::ColorSpace(), GetCameraRotation(facing_mode_), false /* flip_y */,
        reference_time, timestamp);
  }

  while (!video_stream_take_photo_callbacks_.empty()) {
    TakePhotoCallback cb =
        std::move(video_stream_take_photo_callbacks_.front());
    video_stream_take_photo_callbacks_.pop();

    mojom::BlobPtr blob = RotateAndBlobify(
        data, length, selected_video_capability_->supported_format, 0);
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
  media_event->GetStatus(&hr);

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

}  // namespace media
