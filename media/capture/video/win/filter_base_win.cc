// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/filter_base_win.h"

#include "base/notreached.h"

#pragma comment(lib, "strmiids.lib")

namespace media {

// Implement IEnumPins.
class PinEnumerator final : public IEnumPins,
                            public base::RefCounted<PinEnumerator> {
 public:
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
    base::RefCounted<PinEnumerator>::AddRef();
    return 1;
  }

  IFACEMETHODIMP_(ULONG) Release() override {
    base::RefCounted<PinEnumerator>::Release();
    return 1;
  }

  // Implement IEnumPins.
  IFACEMETHODIMP Next(ULONG count, IPin** pins, ULONG* fetched) override {
    ULONG pins_fetched = 0;
    while (pins_fetched < count && filter_->NoOfPins() > index_) {
      IPin* pin = filter_->GetPin(index_++);
      pin->AddRef();
      pins[pins_fetched++] = pin;
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
    PinEnumerator* pin_enum = new PinEnumerator(filter_.get());
    pin_enum->AddRef();
    pin_enum->index_ = index_;
    *clone = pin_enum;
    return S_OK;
  }

 private:
  friend class base::RefCounted<PinEnumerator>;
  ~PinEnumerator() {}

  scoped_refptr<FilterBase> filter_;
  size_t index_;
};

FilterBase::FilterBase() : state_(State_Stopped) {
}

HRESULT FilterBase::EnumPins(IEnumPins** enum_pins) {
  *enum_pins = new PinEnumerator(this);
  (*enum_pins)->AddRef();
  return S_OK;
}

HRESULT FilterBase::FindPin(LPCWSTR id, IPin** pin) {
  return E_NOTIMPL;
}

HRESULT FilterBase::QueryFilterInfo(FILTER_INFO* info) {
  info->pGraph = owning_graph_.Get();
  info->achName[0] = L'\0';
  if (info->pGraph)
    info->pGraph->AddRef();
  return S_OK;
}

HRESULT FilterBase::JoinFilterGraph(IFilterGraph* graph, LPCWSTR name) {
  owning_graph_ = graph;
  return S_OK;
}

HRESULT FilterBase::QueryVendorInfo(LPWSTR* pVendorInfo) {
  return S_OK;
}

// Implement IMediaFilter.
HRESULT FilterBase::Stop() {
  state_ = State_Stopped;
  return S_OK;
}

HRESULT FilterBase::Pause() {
  state_ = State_Paused;
  return S_OK;
}

HRESULT FilterBase::Run(REFERENCE_TIME start) {
  state_ = State_Running;
  return S_OK;
}

HRESULT FilterBase::GetState(DWORD msec_timeout, FILTER_STATE* state) {
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
  return E_NOTIMPL;
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
  base::RefCounted<FilterBase>::AddRef();
  return 1;
}

ULONG STDMETHODCALLTYPE FilterBase::Release() {
  base::RefCounted<FilterBase>::Release();
  return 1;
}

FilterBase::~FilterBase() {
}

}  // namespace media
