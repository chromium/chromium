// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implement a simple base class for a DirectShow input pin. Connection state
// and filter ownership are synchronized and reference counting is thread-safe
// as DirectShow streaming threads may invoke pin methods concurrently.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_PIN_BASE_WIN_H_
#define MEDIA_CAPTURE_VIDEO_WIN_PIN_BASE_WIN_H_

// Avoid including strsafe.h via dshow as it will cause build warnings.
#define NO_DSHOW_STRSAFE
#include <dshow.h>
#include <wrl/client.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

namespace media {

class PinBase : public IPin,
                public IMemInputPin,
                public base::RefCountedThreadSafe<PinBase> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  explicit PinBase(IBaseFilter* owner);

  PinBase(const PinBase&) = delete;
  PinBase& operator=(const PinBase&) = delete;

  // Sets or clears the owning filter. Must be called with nullptr before the
  // owning filter is destroyed.
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
  friend class base::RefCountedThreadSafe<PinBase>;
  virtual ~PinBase();

 private:
  // Owns a deep copy of an AM_MEDIA_TYPE, including its format block and
  // |pUnk|, and frees them when destroyed.
  class OwnedMediaType {
   public:
    OwnedMediaType();
    OwnedMediaType(const OwnedMediaType&) = delete;
    OwnedMediaType& operator=(const OwnedMediaType&) = delete;
    OwnedMediaType(OwnedMediaType&&) = delete;
    OwnedMediaType& operator=(OwnedMediaType&& other);
    ~OwnedMediaType();

    // Replaces the owned type with a deep copy of |source|. On failure, the
    // owned type is left empty.
    HRESULT CopyFrom(const AM_MEDIA_TYPE& source);
    // Writes a deep copy to |target|, which the caller then owns and frees,
    // e.g. with FreeMediaType(). |target| must not own any memory.
    HRESULT CopyTo(AM_MEDIA_TYPE* target) const;

   private:
    // Deep-copies |source| into |target|. |target| must not own any memory.
    static HRESULT CopyMediaType(const AM_MEDIA_TYPE& source,
                                 AM_MEDIA_TYPE* target);

    void Reset();

    AM_MEDIA_TYPE type_ = {};
  };

  base::Lock lock_;
  // Set only while connected.
  OwnedMediaType current_media_type_ GUARDED_BY(lock_);
  Microsoft::WRL::ComPtr<IPin> connected_pin_ GUARDED_BY(lock_);
  // owner_ is the filter owning this pin. We don't reference count it since
  // that would create a circular reference count.
  raw_ptr<IBaseFilter> owner_ GUARDED_BY(lock_);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_PIN_BASE_WIN_H_
