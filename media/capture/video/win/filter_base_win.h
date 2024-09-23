// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implement a simple base class for DirectShow filters. It may only be used in
// a single threaded apartment.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_FILTER_BASE_WIN_H_
#define MEDIA_CAPTURE_VIDEO_WIN_FILTER_BASE_WIN_H_

// Avoid including strsafe.h via dshow as it will cause build warnings.
#define NO_DSHOW_STRSAFE
#include <dshow.h>
#include <stddef.h>
#include <wrl/client.h>

#include "base/memory/ref_counted.h"

namespace media {

class FilterBase : public IBaseFilter, public base::RefCounted<FilterBase> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  FilterBase();

  FilterBase(const FilterBase&) = delete;
  FilterBase& operator=(const FilterBase&) = delete;

  // Number of pins connected to this filter.
  virtual size_t NoOfPins() = 0;
  // Returns the IPin interface pin no index.
  virtual IPin* GetPin(int index) = 0;

  // Inherited from IUnknown.
  IFACEMETHODIMP QueryInterface(REFIID id, void** object_ptr) override;
  IFACEMETHODIMP_(ULONG) AddRef() override;
  IFACEMETHODIMP_(ULONG) Release() override;

  // Inherited from IBaseFilter.
  IFACEMETHODIMP EnumPins(IEnumPins** enum_pins) override;

  IFACEMETHODIMP FindPin(LPCWSTR id, IPin** pin) override;

  IFACEMETHODIMP QueryFilterInfo(FILTER_INFO* info) override;

  IFACEMETHODIMP JoinFilterGraph(IFilterGraph* graph, LPCWSTR name) override;

  IFACEMETHODIMP QueryVendorInfo(LPWSTR* vendor_info) override;

  // Inherited from IMediaFilter.
  IFACEMETHODIMP Stop() override;

  IFACEMETHODIMP Pause() override;

  IFACEMETHODIMP Run(REFERENCE_TIME start) override;

  IFACEMETHODIMP GetState(DWORD msec_timeout, FILTER_STATE* state) override;

  IFACEMETHODIMP SetSyncSource(IReferenceClock* clock) override;

  IFACEMETHODIMP GetSyncSource(IReferenceClock** clock) override;

  // Inherited from IPersistent.
  IFACEMETHODIMP GetClassID(CLSID* class_id) override = 0;

 protected:
  friend class base::RefCounted<FilterBase>;
  virtual ~FilterBase();

 private:
  FILTER_STATE state_;
  Microsoft::WRL::ComPtr<IFilterGraph> owning_graph_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_FILTER_BASE_WIN_H_
