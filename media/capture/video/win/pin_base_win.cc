// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "media/capture/video/win/pin_base_win.h"

#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"

namespace media {

PinBase::OwnedMediaType::OwnedMediaType() = default;

PinBase::OwnedMediaType& PinBase::OwnedMediaType::operator=(
    OwnedMediaType&& other) {
  if (this != &other) {
    Reset();
    type_ = std::exchange(other.type_, {});
  }
  return *this;
}

PinBase::OwnedMediaType::~OwnedMediaType() {
  Reset();
}

HRESULT PinBase::OwnedMediaType::CopyFrom(const AM_MEDIA_TYPE& source) {
  Reset();
  return CopyMediaType(source, &type_);
}

HRESULT PinBase::OwnedMediaType::CopyTo(AM_MEDIA_TYPE* target) const {
  return CopyMediaType(type_, target);
}

// static
HRESULT PinBase::OwnedMediaType::CopyMediaType(const AM_MEDIA_TYPE& source,
                                               AM_MEDIA_TYPE* target) {
  BYTE* format = nullptr;
  if (source.cbFormat && source.pbFormat) {
    format = static_cast<BYTE*>(CoTaskMemAlloc(source.cbFormat));
    if (!format) {
      *target = {};
      return E_OUTOFMEMORY;
    }
    // SAFETY: |source.pbFormat| points to |source.cbFormat| bytes, and
    // |format| was just allocated with that size.
    UNSAFE_BUFFERS(base::span(format, source.cbFormat))
        .copy_from(
            UNSAFE_BUFFERS(base::span(source.pbFormat, source.cbFormat)));
  }
  *target = source;
  target->pbFormat = format;
  target->cbFormat = format ? source.cbFormat : 0;
  if (target->pUnk) {
    target->pUnk->AddRef();
  }
  return S_OK;
}

void PinBase::OwnedMediaType::Reset() {
  if (type_.pbFormat) {
    CoTaskMemFree(type_.pbFormat);
  }
  if (type_.pUnk) {
    type_.pUnk->Release();
  }
  type_ = {};
}

// Implement IEnumMediaTypes.
class TypeEnumerator final : public IEnumMediaTypes,
                             public base::RefCountedThreadSafe<TypeEnumerator> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  explicit TypeEnumerator(PinBase* pin) : pin_(pin), index_(0) {}

  // Implement from IUnknown.
  IFACEMETHODIMP QueryInterface(REFIID iid, void** object_ptr) override {
    if (iid == IID_IEnumMediaTypes || iid == IID_IUnknown) {
      AddRef();
      *object_ptr = static_cast<IEnumMediaTypes*>(this);
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  IFACEMETHODIMP_(ULONG) AddRef() override {
    base::RefCountedThreadSafe<TypeEnumerator>::AddRef();
    return 1;
  }

  IFACEMETHODIMP_(ULONG) Release() override {
    base::RefCountedThreadSafe<TypeEnumerator>::Release();
    return 1;
  }

  // Implement IEnumMediaTypes.
  IFACEMETHODIMP Next(ULONG count,
                      AM_MEDIA_TYPE** types,
                      ULONG* fetched) override {
    ULONG types_fetched = 0;

    while (types_fetched < count) {
      // Allocate AM_MEDIA_TYPE that we will store the media type in.
      AM_MEDIA_TYPE* type = reinterpret_cast<AM_MEDIA_TYPE*>(
          CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE)));
      if (!type) {
        FreeAllocatedMediaTypes(types_fetched, types);
        return E_OUTOFMEMORY;
      }
      UNSAFE_TODO(ZeroMemory(type, sizeof(AM_MEDIA_TYPE)));

      // Allocate a VIDEOINFOHEADER and connect it to the AM_MEDIA_TYPE.
      type->cbFormat = sizeof(VIDEOINFOHEADER);
      BYTE* format =
          reinterpret_cast<BYTE*>(CoTaskMemAlloc(sizeof(VIDEOINFOHEADER)));
      if (!format) {
        CoTaskMemFree(type);
        FreeAllocatedMediaTypes(types_fetched, types);
        return E_OUTOFMEMORY;
      }
      type->pbFormat = format;
      // Get the media type from the pin.
      if (pin_->GetValidMediaType(index_++, type)) {
        UNSAFE_TODO(types[types_fetched++]) = type;
      } else {
        CoTaskMemFree(format);
        CoTaskMemFree(type);
        break;
      }
    }

    if (fetched)
      *fetched = types_fetched;

    return types_fetched == count ? S_OK : S_FALSE;
  }

  IFACEMETHODIMP Skip(ULONG count) override {
    index_ += count;
    return S_OK;
  }

  IFACEMETHODIMP Reset() override {
    index_ = 0;
    return S_OK;
  }

  IFACEMETHODIMP Clone(IEnumMediaTypes** clone) override {
    auto type_enum = base::MakeRefCounted<TypeEnumerator>(pin_.get());
    type_enum->AddRef();
    type_enum->index_ = index_;
    *clone = type_enum.get();
    return S_OK;
  }

 private:
  friend class base::RefCountedThreadSafe<TypeEnumerator>;
  ~TypeEnumerator() = default;

  void FreeAllocatedMediaTypes(ULONG allocated, AM_MEDIA_TYPE** types) {
    for (ULONG i = 0; i < allocated; ++i) {
      CoTaskMemFree(UNSAFE_TODO(types[i]->pbFormat));
      CoTaskMemFree(UNSAFE_TODO(types[i]));
    }
  }

  scoped_refptr<PinBase> pin_;
  int index_;
};

PinBase::PinBase(IBaseFilter* owner) : owner_(owner) {}

void PinBase::SetOwner(IBaseFilter* owner) {
  base::AutoLock lock(lock_);
  owner_ = owner;
}

// Only called on output pins; PinBase implements input pins.
HRESULT PinBase::Connect(IPin* receive_pin, const AM_MEDIA_TYPE* media_type) {
  return E_UNEXPECTED;
}

// Called from an output pin on an input pin to establish a connection.
HRESULT PinBase::ReceiveConnection(IPin* connector,
                                   const AM_MEDIA_TYPE* media_type) {
  if (!connector || !media_type) {
    return E_POINTER;
  }
  {
    base::AutoLock lock(lock_);
    if (connected_pin_) {
      return VFW_E_ALREADY_CONNECTED;
    }
  }
  if (!IsMediaTypeValid(media_type)) {
    return VFW_E_TYPE_NOT_ACCEPTED;
  }

  OwnedMediaType new_type;
  HRESULT hr = new_type.CopyFrom(*media_type);
  if (FAILED(hr)) {
    return hr;
  }
  Microsoft::WRL::ComPtr<IPin> new_pin(connector);
  {
    // Hold |lock_| only briefly, and don't destroy media types under it: their
    // destructors run custom filter code, which could deadlock.
    base::AutoLock lock(lock_);
    if (!connected_pin_) {
      current_media_type_ = std::move(new_type);
      connected_pin_ = std::move(new_pin);
      return S_OK;
    }
  }
  return VFW_E_ALREADY_CONNECTED;
}

HRESULT PinBase::Disconnect() {
  Microsoft::WRL::ComPtr<IPin> old_pin;
  OwnedMediaType old_type;
  {
    // Hold |lock_| only briefly, and don't destroy media types under it: their
    // destructors run custom filter code, which could deadlock.
    base::AutoLock lock(lock_);
    if (!connected_pin_) {
      return S_FALSE;
    }
    old_type = std::move(current_media_type_);
    old_pin = std::move(connected_pin_);
  }
  old_pin.Reset();
  return S_OK;
}

HRESULT PinBase::ConnectedTo(IPin** pin) {
  base::AutoLock lock(lock_);
  *pin = connected_pin_.Get();
  if (!connected_pin_) {
    return VFW_E_NOT_CONNECTED;
  }

  connected_pin_->AddRef();
  return S_OK;
}

HRESULT PinBase::ConnectionMediaType(AM_MEDIA_TYPE* media_type) {
  base::AutoLock lock(lock_);
  if (!connected_pin_) {
    *media_type = {};
    return VFW_E_NOT_CONNECTED;
  }
  return current_media_type_.CopyTo(media_type);
}

HRESULT PinBase::QueryPinInfo(PIN_INFO* info) {
  info->dir = PINDIR_INPUT;
  info->achName[0] = L'\0';
  base::AutoLock lock(lock_);
  info->pFilter = owner_;
  if (owner_) {
    owner_->AddRef();
  }
  return S_OK;
}

HRESULT PinBase::QueryDirection(PIN_DIRECTION* pin_dir) {
  *pin_dir = PINDIR_INPUT;
  return S_OK;
}

HRESULT PinBase::QueryId(LPWSTR* id) {
  NOTREACHED();
}

HRESULT PinBase::QueryAccept(const AM_MEDIA_TYPE* media_type) {
  return S_FALSE;
}

HRESULT PinBase::EnumMediaTypes(IEnumMediaTypes** types) {
  auto type_enum = base::MakeRefCounted<TypeEnumerator>(this);
  type_enum->AddRef();
  *types = type_enum.get();
  return S_OK;
}

HRESULT PinBase::QueryInternalConnections(IPin** pins, ULONG* no_pins) {
  return E_NOTIMPL;
}

HRESULT PinBase::EndOfStream() {
  return S_OK;
}

HRESULT PinBase::BeginFlush() {
  return S_OK;
}

HRESULT PinBase::EndFlush() {
  return S_OK;
}

HRESULT PinBase::NewSegment(REFERENCE_TIME start,
                            REFERENCE_TIME stop,
                            double rate) {
  NOTREACHED();
}

// Inherited from IMemInputPin.
HRESULT PinBase::GetAllocator(IMemAllocator** allocator) {
  return VFW_E_NO_ALLOCATOR;
}

HRESULT PinBase::NotifyAllocator(IMemAllocator* allocator, BOOL read_only) {
  return S_OK;
}

HRESULT PinBase::GetAllocatorRequirements(ALLOCATOR_PROPERTIES* properties) {
  return E_NOTIMPL;
}

HRESULT PinBase::ReceiveMultiple(IMediaSample** samples,
                                 long sample_count,
                                 long* processed) {
  DCHECK(samples);

  HRESULT hr = S_OK;
  *processed = 0;
  while (sample_count--) {
    hr = Receive(UNSAFE_TODO(samples[*processed]));
    // S_FALSE means don't send any more.
    if (hr != S_OK)
      break;
    ++(*processed);
  }
  return hr;
}

HRESULT PinBase::ReceiveCanBlock() {
  return S_FALSE;
}

// Inherited from IUnknown.
HRESULT PinBase::QueryInterface(REFIID id, void** object_ptr) {
  if (id == IID_IPin || id == IID_IUnknown) {
    *object_ptr = static_cast<IPin*>(this);
  } else if (id == IID_IMemInputPin) {
    *object_ptr = static_cast<IMemInputPin*>(this);
  } else {
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

ULONG PinBase::AddRef() {
  base::RefCountedThreadSafe<PinBase>::AddRef();
  return 1;
}

ULONG PinBase::Release() {
  base::RefCountedThreadSafe<PinBase>::Release();
  return 1;
}

PinBase::~PinBase() {
}

}  // namespace media
