// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mac/video_capture_device_decklink_mac.h"

#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/lock.h"
#include "third_party/decklink/mac/include/DeckLinkAPI.h"

namespace {

// DeckLink SDK uses ComPtr-style APIs. Microsoft::WRL::ComPtr<> is only
// available for Windows builds. This provides a subset of the methods required
// for ref counting.
template <class T>
class ScopedDeckLinkPtr : public scoped_refptr<T> {
 private:
  using scoped_refptr<T>::ptr_;

 public:
  T** Receive() {
    DCHECK(!ptr_) << "Object leak. Pointer must be NULL";
    return &ptr_;
  }

  void** ReceiveVoid() { return reinterpret_cast<void**>(Receive()); }

  void Release() {
    if (ptr_ != NULL) {
      ptr_->Release();
      ptr_ = NULL;
    }
  }
};

// This class is used to interact directly with DeckLink SDK for video capture.
// Implements the reference counted interface IUnknown. Has a weak reference to
// VideoCaptureDeviceDeckLinkMac for sending captured frames, error messages and
// logs.
class DeckLinkCaptureDelegate
    : public IDeckLinkInputCallback,
      public base::RefCountedThreadSafe<DeckLinkCaptureDelegate> {
 public:
  DeckLinkCaptureDelegate(
      const media::VideoCaptureDeviceDescriptor& device_descriptor,
      media::VideoCaptureDeviceDeckLinkMac* frame_receiver);

  void AllocateAndStart(const media::VideoCaptureParams& params);
  void StopAndDeAllocate();

  // Remove the VideoCaptureDeviceDeckLinkMac's weak reference.
  void ResetVideoCaptureDeviceReference();

 private:
  // IDeckLinkInputCallback interface implementation.
  HRESULT VideoInputFormatChanged(
      BMDVideoInputFormatChangedEvents notification_events,
      IDeckLinkDisplayMode* new_display_mode,
      BMDDetectedVideoInputFormatFlags detected_signal_flags) override;
  HRESULT VideoInputFrameArrived(
      IDeckLinkVideoInputFrame* video_frame,
      IDeckLinkAudioInputPacket* audio_packet) override;

  // IUnknown interface implementation.
  HRESULT QueryInterface(REFIID iid, void** ppv) override;
  ULONG AddRef() override;
  ULONG Release() override;

  // Forwarder to VideoCaptureDeviceDeckLinkMac::SendErrorString().
  void SendErrorString(media::VideoCaptureError error,
                       const base::Location& from_here,
                       const std::string& reason);

  // Forwarder to VideoCaptureDeviceDeckLinkMac::SendLogString().
  void SendLogString(const std::string& message);

  const media::VideoCaptureDeviceDescriptor device_descriptor_;

  // Protects concurrent setting and using of |frame_receiver_|.
  base::Lock lock_;
  // Weak reference to the captured frames client, used also for error messages
  // and logging. Initialized on construction and used until cleared by calling
  // ResetVideoCaptureDeviceReference().
  media::VideoCaptureDeviceDeckLinkMac* frame_receiver_;

  // This is used to control the video capturing device input interface.
  ScopedDeckLinkPtr<IDeckLinkInput> decklink_input_;
  // |decklink_| represents a physical device attached to the host.
  ScopedDeckLinkPtr<IDeckLink> decklink_;

  base::TimeTicks first_ref_time_;

  // Checks for Device (a.k.a. Audio) thread.
  base::ThreadChecker thread_checker_;

  friend class scoped_refptr<DeckLinkCaptureDelegate>;
  friend class base::RefCountedThreadSafe<DeckLinkCaptureDelegate>;

  ~DeckLinkCaptureDelegate() override;

  DISALLOW_COPY_AND_ASSIGN(DeckLinkCaptureDelegate);
};

static float GetDisplayModeFrameRate(
    const ScopedDeckLinkPtr<IDeckLinkDisplayMode>& display_mode) {
  BMDTimeValue time_value, time_scale;
  float display_mode_frame_rate = 0.0f;
  if (display_mode->GetFrameRate(&time_value, &time_scale) == S_OK &&
      time_value > 0) {
    display_mode_frame_rate = static_cast<float>(time_scale) / time_value;
  }
  // Interlaced formats are going to be marked as double the frame rate,
  // which follows the general naming convention.
  if (display_mode->GetFieldDominance() == bmdLowerFieldFirst ||
      display_mode->GetFieldDominance() == bmdUpperFieldFirst) {
    display_mode_frame_rate *= 2.0f;
  }
  return display_mode_frame_rate;
}

DeckLinkCaptureDelegate::DeckLinkCaptureDelegate(
    const media::VideoCaptureDeviceDescriptor& device_descriptor,
    media::VideoCaptureDeviceDeckLinkMac* frame_receiver)
    : device_descriptor_(device_descriptor), frame_receiver_(frame_receiver) {}

DeckLinkCaptureDelegate::~DeckLinkCaptureDelegate() {
}

void DeckLinkCaptureDelegate::AllocateAndStart(
    const media::VideoCaptureParams& params) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_refptr<IDeckLinkIterator> decklink_iter(
      CreateDeckLinkIteratorInstance());
  DLOG_IF(ERROR, !decklink_iter.get()) << "Error creating DeckLink iterator";
  if (!decklink_iter.get())
    return;

  ScopedDeckLinkPtr<IDeckLink> decklink_local;
  while (decklink_iter->Next(decklink_local.Receive()) == S_OK) {
    CFStringRef device_model_name = NULL;
    if ((decklink_local->GetModelName(&device_model_name) == S_OK) ||
        (device_descriptor_.device_id ==
         base::SysCFStringRefToUTF8(device_model_name))) {
      break;
    }
  }
  if (!decklink_local.get()) {
    SendErrorString(
        media::VideoCaptureError::kMacDeckLinkDeviceIdNotFoundInTheSystem,
        FROM_HERE, "Device id not found in the system");
    return;
  }

  ScopedDeckLinkPtr<IDeckLinkInput> decklink_input_local;
  if (decklink_local->QueryInterface(
          IID_IDeckLinkInput, decklink_input_local.ReceiveVoid()) != S_OK) {
    SendErrorString(
        media::VideoCaptureError::kMacDeckLinkErrorQueryingInputInterface,
        FROM_HERE, "Error querying input interface.");
    return;
  }

  ScopedDeckLinkPtr<IDeckLinkDisplayModeIterator> display_mode_iter;
  if (decklink_input_local->GetDisplayModeIterator(
          display_mode_iter.Receive()) != S_OK) {
    SendErrorString(
        media::VideoCaptureError::kMacDeckLinkErrorCreatingDisplayModeIterator,
        FROM_HERE, "Error creating Display Mode Iterator");
    return;
  }

  ScopedDeckLinkPtr<IDeckLinkDisplayMode> chosen_display_mode;
  ScopedDeckLinkPtr<IDeckLinkDisplayMode> display_mode;
  float min_diff = FLT_MAX;
  while (display_mode_iter->Next(display_mode.Receive()) == S_OK) {
    const float diff = labs(display_mode->GetWidth() -
                            params.requested_format.frame_size.width()) +
                       labs(params.requested_format.frame_size.height() -
                            display_mode->GetHeight()) +
                       fabs(params.requested_format.frame_rate -
                            GetDisplayModeFrameRate(display_mode));
    if (diff < min_diff) {
      chosen_display_mode = display_mode;
      min_diff = diff;
    }
    display_mode.Release();
  }
  if (!chosen_display_mode.get()) {
    SendErrorString(
        media::VideoCaptureError::kMacDeckLinkCouldNotFindADisplayMode,
        FROM_HERE, "Could not find a display mode");
    return;
  }
#if !defined(NDEBUG)
  DVLOG(1) << "Requested format: "
           << media::VideoCaptureFormat::ToString(params.requested_format);
  CFStringRef format_name = NULL;
  if (chosen_display_mode->GetName(&format_name) == S_OK)
    DVLOG(1) << "Chosen format: " << base::SysCFStringRefToUTF8(format_name);
#endif

  // Enable video input. Configure for no input video format change detection,
  // this in turn will disable calls to VideoInputFormatChanged().
  if (decklink_input_local->EnableVideoInput(
          chosen_display_mode->GetDisplayMode(), bmdFormat8BitYUV,
          bmdVideoInputFlagDefault) != S_OK) {
    SendErrorString(media::VideoCaptureError::
                        kMacDeckLinkCouldNotSelectTheVideoFormatWeLike,
                    FROM_HERE, "Could not select the video format we like.");
    return;
  }

  decklink_input_local->SetCallback(this);
  if (decklink_input_local->StartStreams() != S_OK)
    SendErrorString(
        media::VideoCaptureError::kMacDeckLinkCouldNotStartCapturing, FROM_HERE,
        "Could not start capturing");

  if (frame_receiver_)
    frame_receiver_->ReportStarted();

  decklink_.swap(decklink_local);
  decklink_input_.swap(decklink_input_local);
}

void DeckLinkCaptureDelegate::StopAndDeAllocate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!decklink_input_.get())
    return;
  if (decklink_input_->StopStreams() != S_OK)
    SendLogString("Problem stopping capture.");
  decklink_input_->SetCallback(NULL);
  decklink_input_->DisableVideoInput();
  decklink_input_.Release();
  decklink_.Release();
  ResetVideoCaptureDeviceReference();
}

HRESULT DeckLinkCaptureDelegate::VideoInputFormatChanged(
    BMDVideoInputFormatChangedEvents notification_events,
    IDeckLinkDisplayMode* new_display_mode,
    BMDDetectedVideoInputFormatFlags detected_signal_flags) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return S_OK;
}

HRESULT DeckLinkCaptureDelegate::VideoInputFrameArrived(
    IDeckLinkVideoInputFrame* video_frame,
    IDeckLinkAudioInputPacket* /* audio_packet */) {
  // Capture frames are manipulated as an IDeckLinkVideoFrame.
  uint8_t* video_data = NULL;
  video_frame->GetBytes(reinterpret_cast<void**>(&video_data));

  media::VideoPixelFormat pixel_format =
      media::PIXEL_FORMAT_UNKNOWN;
  switch (video_frame->GetPixelFormat()) {
    case bmdFormat8BitARGB:
      pixel_format = media::PIXEL_FORMAT_ARGB;
      break;
    default:
      SendErrorString(
          media::VideoCaptureError::kMacDeckLinkUnsupportedPixelFormat,
          FROM_HERE, "Unsupported pixel format");
      break;
  }

  const media::VideoCaptureFormat capture_format(
      gfx::Size(video_frame->GetWidth(), video_frame->GetHeight()),
      0.0f,  // Frame rate is not needed for captured data callback.
      pixel_format);
  base::TimeTicks now = base::TimeTicks::Now();
  if (first_ref_time_.is_null())
    first_ref_time_ = now;
  base::AutoLock lock(lock_);
  if (frame_receiver_) {
    const BMDTimeScale micros_time_scale = base::Time::kMicrosecondsPerSecond;
    BMDTimeValue frame_time;
    BMDTimeValue frame_duration;
    base::TimeDelta timestamp;
    if (SUCCEEDED(video_frame->GetStreamTime(&frame_time, &frame_duration,
                                             micros_time_scale))) {
      timestamp = base::TimeDelta::FromMicroseconds(frame_time);
    } else {
      timestamp = now - first_ref_time_;
    }
    // TODO(julien.isorce): Build a gfx::ColorSpace from DeckLink API, .i.e
    // using BMDDisplayModeFlags or BMDDeckLinkFrameMetadataID. See
    // http://crbug.com/959953.
    frame_receiver_->OnIncomingCapturedData(
        video_data, video_frame->GetRowBytes() * video_frame->GetHeight(),
        capture_format, gfx::ColorSpace(),
        0,      // Rotation.
        false,  // Vertical flip.
        now, timestamp);
  }
  return S_OK;
}

HRESULT DeckLinkCaptureDelegate::QueryInterface(REFIID iid, void** ppv) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CFUUIDBytes iunknown = CFUUIDGetUUIDBytes(IUnknownUUID);
  if (memcmp(&iid, &iunknown, sizeof(REFIID)) == 0 ||
      memcmp(&iid, &IID_IDeckLinkInputCallback, sizeof(REFIID)) == 0) {
    *ppv = static_cast<IDeckLinkInputCallback*>(this);
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

ULONG DeckLinkCaptureDelegate::AddRef() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::RefCountedThreadSafe<DeckLinkCaptureDelegate>::AddRef();
  return 1;
}

ULONG DeckLinkCaptureDelegate::Release() {
  DCHECK(thread_checker_.CalledOnValidThread());
  bool ret_value = !HasOneRef();
  base::RefCountedThreadSafe<DeckLinkCaptureDelegate>::Release();
  return ret_value;
}

void DeckLinkCaptureDelegate::SendErrorString(media::VideoCaptureError error,
                                              const base::Location& from_here,
                                              const std::string& reason) {
  base::AutoLock lock(lock_);
  if (frame_receiver_)
    frame_receiver_->SendErrorString(error, from_here, reason);
}

void DeckLinkCaptureDelegate::SendLogString(const std::string& message) {
  base::AutoLock lock(lock_);
  if (frame_receiver_)
    frame_receiver_->SendLogString(message);
}

void DeckLinkCaptureDelegate::ResetVideoCaptureDeviceReference() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);
  frame_receiver_ = NULL;
}

}  // namespace

namespace media {

static std::string JoinDeviceNameAndFormat(CFStringRef name,
                                           CFStringRef format) {
  return base::SysCFStringRefToUTF8(name) + " - " +
         base::SysCFStringRefToUTF8(format);
}

// static
void VideoCaptureDeviceDeckLinkMac::EnumerateDevices(
    VideoCaptureDeviceDescriptors* device_descriptors) {
  scoped_refptr<IDeckLinkIterator> decklink_iter(
      CreateDeckLinkIteratorInstance());
  // At this point, not being able to create a DeckLink iterator means that
  // there are no Blackmagic DeckLink devices in the system, don't print error.
  DVLOG_IF(1, !decklink_iter.get()) << "Could not create DeckLink iterator";
  if (!decklink_iter.get())
    return;

  ScopedDeckLinkPtr<IDeckLink> decklink;
  while (decklink_iter->Next(decklink.Receive()) == S_OK) {
    ScopedDeckLinkPtr<IDeckLink> decklink_local;
    decklink_local.swap(decklink);

    CFStringRef device_model_name = NULL;
    HRESULT hr = decklink_local->GetModelName(&device_model_name);
    DVLOG_IF(1, hr != S_OK) << "Error reading Blackmagic device model name";
    CFStringRef device_display_name = NULL;
    hr = decklink_local->GetDisplayName(&device_display_name);
    DVLOG_IF(1, hr != S_OK) << "Error reading Blackmagic device display name";
    DVLOG_IF(1, hr == S_OK) << "Blackmagic device found with name: "
                            << base::SysCFStringRefToUTF8(device_display_name);

    if (!device_model_name && !device_display_name)
      continue;

    ScopedDeckLinkPtr<IDeckLinkInput> decklink_input;
    if (decklink_local->QueryInterface(IID_IDeckLinkInput,
                                       decklink_input.ReceiveVoid()) != S_OK) {
      DLOG(ERROR) << "Error Blackmagic querying input interface.";
      return;
    }

    ScopedDeckLinkPtr<IDeckLinkDisplayModeIterator> display_mode_iter;
    if (decklink_input->GetDisplayModeIterator(display_mode_iter.Receive()) !=
        S_OK) {
      continue;
    }

    ScopedDeckLinkPtr<IDeckLinkDisplayMode> display_mode;
    while (display_mode_iter->Next(display_mode.Receive()) == S_OK) {
      CFStringRef format_name = NULL;
      if (display_mode->GetName(&format_name) == S_OK) {
        VideoCaptureDeviceDescriptor descriptor;
        descriptor.set_display_name(
            JoinDeviceNameAndFormat(device_display_name, format_name));
        descriptor.device_id =
            JoinDeviceNameAndFormat(device_model_name, format_name);
        descriptor.capture_api = VideoCaptureApi::MACOSX_DECKLINK;
        descriptor.transport_type = VideoCaptureTransportType::OTHER_TRANSPORT;
        device_descriptors->push_back(descriptor);
        DVLOG(1) << "Blackmagic camera enumerated: "
                 << descriptor.display_name();
      }
      display_mode.Release();
    }
  }
}

// static
void VideoCaptureDeviceDeckLinkMac::EnumerateDeviceCapabilities(
    const VideoCaptureDeviceDescriptor& device,
    VideoCaptureFormats* supported_formats) {
  scoped_refptr<IDeckLinkIterator> decklink_iter(
      CreateDeckLinkIteratorInstance());
  DLOG_IF(ERROR, !decklink_iter.get()) << "Error creating DeckLink iterator";
  if (!decklink_iter.get())
    return;

  ScopedDeckLinkPtr<IDeckLink> decklink;
  while (decklink_iter->Next(decklink.Receive()) == S_OK) {
    ScopedDeckLinkPtr<IDeckLink> decklink_local;
    decklink_local.swap(decklink);

    ScopedDeckLinkPtr<IDeckLinkInput> decklink_input;
    if (decklink_local->QueryInterface(IID_IDeckLinkInput,
                                       decklink_input.ReceiveVoid()) != S_OK) {
      DLOG(ERROR) << "Error Blackmagic querying input interface.";
      return;
    }

    ScopedDeckLinkPtr<IDeckLinkDisplayModeIterator> display_mode_iter;
    if (decklink_input->GetDisplayModeIterator(display_mode_iter.Receive()) !=
        S_OK) {
      continue;
    }

    CFStringRef device_model_name = NULL;
    if (decklink_local->GetModelName(&device_model_name) != S_OK)
      continue;

    ScopedDeckLinkPtr<IDeckLinkDisplayMode> display_mode;
    while (display_mode_iter->Next(display_mode.Receive()) == S_OK) {
      CFStringRef format_name = NULL;
      if (display_mode->GetName(&format_name) == S_OK &&
          device.device_id !=
              JoinDeviceNameAndFormat(device_model_name, format_name)) {
        display_mode.Release();
        continue;
      }

      // IDeckLinkDisplayMode does not have information on pixel format, this
      // is only available on capture.
      const media::VideoCaptureFormat format(
          gfx::Size(display_mode->GetWidth(), display_mode->GetHeight()),
          GetDisplayModeFrameRate(display_mode), PIXEL_FORMAT_UNKNOWN);
      supported_formats->push_back(format);
      DVLOG(2) << device.display_name() << " "
               << VideoCaptureFormat::ToString(format);
      display_mode.Release();
    }
    return;
  }
}

VideoCaptureDeviceDeckLinkMac::VideoCaptureDeviceDeckLinkMac(
    const VideoCaptureDeviceDescriptor& device_descriptor)
    : decklink_capture_delegate_(
          new DeckLinkCaptureDelegate(device_descriptor, this)) {}

VideoCaptureDeviceDeckLinkMac::~VideoCaptureDeviceDeckLinkMac() {
  decklink_capture_delegate_->ResetVideoCaptureDeviceReference();
}

void VideoCaptureDeviceDeckLinkMac::OnIncomingCapturedData(
    const uint8_t* data,
    size_t length,
    const VideoCaptureFormat& frame_format,
    const gfx::ColorSpace& color_space,
    int rotation,  // Clockwise.
    bool flip_y,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp) {
  base::AutoLock lock(lock_);
  if (!client_)
    return;
  client_->OnIncomingCapturedData(data, length, frame_format, color_space,
                                  rotation, flip_y, reference_time, timestamp);
}

void VideoCaptureDeviceDeckLinkMac::SendErrorString(
    VideoCaptureError error,
    const base::Location& from_here,
    const std::string& reason) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);
  if (client_)
    client_->OnError(error, from_here, reason);
}

void VideoCaptureDeviceDeckLinkMac::SendLogString(const std::string& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);
  if (client_)
    client_->OnLog(message);
}

void VideoCaptureDeviceDeckLinkMac::ReportStarted() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);
  if (client_)
    client_->OnStarted();
}

void VideoCaptureDeviceDeckLinkMac::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  client_ = std::move(client);
  if (decklink_capture_delegate_.get())
    decklink_capture_delegate_->AllocateAndStart(params);
}

void VideoCaptureDeviceDeckLinkMac::StopAndDeAllocate() {
  if (decklink_capture_delegate_.get())
    decklink_capture_delegate_->StopAndDeAllocate();
}

}  // namespace media
