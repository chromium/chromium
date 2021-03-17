// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implement a simple base class for a DirectShow input pin. It may only be
// used in a single threaded apartment.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_PIN_BASE_WIN_H_
#define MEDIA_CAPTURE_VIDEO_WIN_PIN_BASE_WIN_H_

// Avoid including strsafe.h via dshow as it will cause build warnings.
#define NO_DSHOW_STRSAFE
#include <dshow.h>
#include <wrl/client.h>

#include "base/memory/ref_counted.h"

namespace media {

class PinBase : public IPin,
                public IMemInputPin,
                public base::RefCounted<PinBase> {
 public:
  explicit PinBase(IBaseFilter* owner);

  // Function used for changing the owner.
  // If the owner is deleted the owner should first call this function
  // with owner = NULL.
  void SetOwner(IBaseFilter* owner);

  // Checks if a media type is acceptable. This is called when this pin is
  // connected to an output pin. Must return true if the media type is
  // acceptable, false otherwise.
  virtual bool IsMediaTypeValid(const AM_MEDIA_TYPE* media_type) = 0;

  // Enumerates valid media types.
  virtual bool GetValidMediaType(int index, AM_MEDIA_TYPE* media_type) = 0;

  // Called when new media is received. Note that this is not on the same
  // thread as where the pin is created.
  IFACEMETHODIMP Receive(IMediaSample* sample) override = 0;

  IFACEMETHODIMP Connect(IPin* receive_pin,
                         const AM_MEDIA_TYPE* media_type) override;

  IFACEMETHODIMP ReceiveConnection(IPin* connector,
                                   const AM_MEDIA_TYPE* media_type) override;

  IFACEMETHODIMP Disconnect() override;

  IFACEMETHODIMP ConnectedTo(IPin** pin) override;

  IFACEMETHODIMP ConnectionMediaType(AM_MEDIA_TYPE* media_type) override;

  IFACEMETHODIMP QueryPinInfo(PIN_INFO* info) override;

  IFACEMETHODIMP QueryDirection(PIN_DIRECTION* pin_dir) override;

  IFACEMETHODIMP QueryId(LPWSTR* id) override;

  IFACEMETHODIMP QueryAccept(const AM_MEDIA_TYPE* media_type) override;

  IFACEMETHODIMP EnumMediaTypes(IEnumMediaTypes** types) override;

  IFACEMETHODIMP QueryInternalConnections(IPin** pins, ULONG* no_pins) override;

  IFACEMETHODIMP EndOfStream() override;

  IFACEMETHODIMP BeginFlush() override;

  IFACEMETHODIMP EndFlush() override;

  IFACEMETHODIMP NewSegment(REFERENCE_TIME start,
                            REFERENCE_TIME stop,
                            double dRate) override;

  // Inherited from IMemInputPin.
  IFACEMETHODIMP GetAllocator(IMemAllocator** allocator) override;

  IFACEMETHODIMP NotifyAllocator(IMemAllocator* allocator,
                                 BOOL read_only) override;

  IFACEMETHODIMP GetAllocatorRequirements(
      ALLOCATOR_PROPERTIES* properties) override;

  IFACEMETHODIMP ReceiveMultiple(IMediaSample** samples,
                                 long sample_count,
                                 long* processed) override;
  IFACEMETHODIMP ReceiveCanBlock() override;

  // Inherited from IUnknown.
  IFACEMETHODIMP QueryInterface(REFIID id, void** object_ptr) override;

  IFACEMETHODIMP_(ULONG) AddRef() override;

  IFACEMETHODIMP_(ULONG) Release() override;

 protected:
  friend class base::RefCounted<PinBase>;
  virtual ~PinBase();

 private:
  AM_MEDIA_TYPE current_media_type_;
  Microsoft::WRL::ComPtr<IPin> connected_pin_;
  // owner_ is the filter owning this pin. We don't reference count it since
  // that would create a circular reference count.
  IBaseFilter* owner_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_PIN_BASE_WIN_H_
