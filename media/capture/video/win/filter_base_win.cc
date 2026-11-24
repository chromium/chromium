// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "media/capture/video/win/filter_base_win.h"

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"

#pragma comment(lib, "strmiids.lib")

namespace media {

// Implement IEnumPins.
class PinEnumerator final : public IEnumPins,
                            public base::RefCountedThreadSafe<PinEnumerator> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  explicit PinEnumerator(FilterBase* filter) : filter_(filter), index_(0) {}

  // IUnknown implementation.
  IFACEMETHODIMP QueryInterface(REFIID iid, void** object_ptr) override {
    if (iid == IID_IEnumPins || iid == IID_IUnknown) {
      AddRef();
      *object_ptr = static_cast<IEnumPins*>(this);
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  IFACEMETHODIMP_(ULONG) AddRef() override {
    base::RefCountedThreadSafe<PinEnumerator>::AddRef();
    return 1;
  }

  IFACEMETHODIMP_(ULONG) Release() override {
    base::RefCountedThreadSafe<PinEnumerator>::Release();
    return 1;
  }

  // Implement IEnumPins.
  IFACEMETHODIMP Next(ULONG count, IPin** pins, ULONG* fetched) override {
    ULONG pins_fetched = 0;
    while (pins_fetched < count && filter_->NoOfPins() > index_) {
      IPin* pin = filter_->GetPin(index_++);
      pin->AddRef();
      UNSAFE_TODO(pins[pins_fetched++]) = pin;
    }

    if (fetched)
      *fetched = pins_fetched;

    return pins_fetched == count ? S_OK : S_FALSE;
  }

  IFACEMETHODIMP Skip(ULONG count) override {
    if (filter_->NoOfPins() - index_ > count) {
      index_ += count;
      return S_OK;
    }
    index_ = 0;
    return S_FALSE;
  }

  IFACEMETHODIMP Reset() override {
    index_ = 0;
    return S_OK;
  }

  IFACEMETHODIMP Clone(IEnumPins** clone) override {
    auto pin_enum = base::MakeRefCounted<PinEnumerator>(filter_.get());
    pin_enum->AddRef();
    pin_enum->index_ = index_;
    *clone = pin_enum.get();
    return S_OK;
  }

 private:
  friend class base::RefCountedThreadSafe<PinEnumerator>;
  ~PinEnumerator() = default;

  scoped_refptr<FilterBase> filter_;
  size_t index_;
};

FilterBase::FilterBase() : state_(State_Stopped) {
}

HRESULT FilterBase::EnumPins(IEnumPins** enum_pins) {
  auto pin_enum = base::MakeRefCounted<PinEnumerator>(this);
  pin_enum->AddRef();
  *enum_pins = pin_enum.get();
  return S_OK;
}

HRESULT FilterBase::FindPin(LPCWSTR id, IPin** pin) {
  return E_NOTIMPL;
}

HRESULT FilterBase::QueryFilterInfo(FILTER_INFO* info) {
  info->achName[0] = L'\0';
  base::AutoLock lock(lock_);
  info->pGraph = owning_graph_.Get();
  if (info->pGraph) {
    info->pGraph->AddRef();
  }
  return S_OK;
}

HRESULT FilterBase::JoinFilterGraph(IFilterGraph* graph, LPCWSTR name) {
  // Take and drop the graph references outside |lock_|.
  Microsoft::WRL::ComPtr<IFilterGraph> other_graph(graph);
  {
    base::AutoLock lock(lock_);
    owning_graph_.Swap(other_graph);
  }
  return S_OK;
}

HRESULT FilterBase::QueryVendorInfo(LPWSTR* pVendorInfo) {
  return S_OK;
}

// Implement IMediaFilter.
HRESULT FilterBase::Stop() {
  base::AutoLock lock(lock_);
  state_ = State_Stopped;
  return S_OK;
}

HRESULT FilterBase::Pause() {
  base::AutoLock lock(lock_);
  state_ = State_Paused;
  return S_OK;
}

HRESULT FilterBase::Run(REFERENCE_TIME start) {
  base::AutoLock lock(lock_);
  state_ = State_Running;
  return S_OK;
}

HRESULT FilterBase::GetState(DWORD msec_timeout, FILTER_STATE* state) {
  base::AutoLock lock(lock_);
  *state = state_;
  return S_OK;
}

HRESULT FilterBase::SetSyncSource(IReferenceClock* clock) {
  return S_OK;
}

HRESULT FilterBase::GetSyncSource(IReferenceClock** clock) {
  return E_NOTIMPL;
}

// Implement from IPersistent.
HRESULT FilterBase::GetClassID(CLSID* class_id) {
  NOTREACHED();
}

// Implement IUnknown.
HRESULT FilterBase::QueryInterface(REFIID id, void** object_ptr) {
  if (id == IID_IMediaFilter || id == IID_IUnknown) {
    *object_ptr = static_cast<IMediaFilter*>(this);
  } else if (id == IID_IPersist) {
    *object_ptr = static_cast<IPersist*>(this);
  } else {
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

ULONG STDMETHODCALLTYPE FilterBase::AddRef() {
  base::RefCountedThreadSafe<FilterBase>::AddRef();
  return 1;
}

ULONG STDMETHODCALLTYPE FilterBase::Release() {
  base::RefCountedThreadSafe<FilterBase>::Release();
  return 1;
}

FilterBase::~FilterBase() {
}

}  // namespace media
