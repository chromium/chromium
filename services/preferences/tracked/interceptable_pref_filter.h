// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_INTERCEPTABLE_PREF_FILTER_H_
#define SERVICES_PREFERENCES_TRACKED_INTERCEPTABLE_PREF_FILTER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/prefs/pref_filter.h"

// A partial implementation of a PrefFilter whose FilterOnLoad call may be
// intercepted by a FilterOnLoadInterceptor. Implementations of
// InterceptablePrefFilter are expected to override FinalizeFilterOnLoad rather
// than re-overriding FilterOnLoad.
class InterceptablePrefFilter
    : public PrefFilter,
      public base::SupportsWeakPtr<InterceptablePrefFilter> {
 public:
  // A callback to be invoked by a FilterOnLoadInterceptor when its ready to
  // hand back the |prefs| it was handed for early filtering. |prefs_altered|
  // indicates whether the |prefs| were actually altered by the
  // FilterOnLoadInterceptor before being handed back.
  using FinalizeFilterOnLoadCallback =
      base::OnceCallback<void(std::unique_ptr<base::DictionaryValue> prefs,
                              bool prefs_altered)>;

  // A callback to be invoked from FilterOnLoad. It takes ownership of prefs
  // and may modify them before handing them back to this
  // InterceptablePrefFilter via |finalize_filter_on_load|.
  using FilterOnLoadInterceptor = base::OnceCallback<void(
      FinalizeFilterOnLoadCallback finalize_filter_on_load,
      std::unique_ptr<base::DictionaryValue> prefs)>;

  InterceptablePrefFilter();
  ~InterceptablePrefFilter() override;

  // PrefFilter partial implementation.
  void FilterOnLoad(
      PostFilterOnLoadCallback post_filter_on_load_callback,
      std::unique_ptr<base::DictionaryValue> pref_store_contents) override;

  // Registers |filter_on_load_interceptor| to intercept the next FilterOnLoad
  // event. At most one FilterOnLoadInterceptor should be registered per
  // PrefFilter.
  void InterceptNextFilterOnLoad(
      FilterOnLoadInterceptor filter_on_load_interceptor);

  void OnStoreDeletionFromDisk() override;

 private:
  // Does any extra filtering required by the implementation of this
  // InterceptablePrefFilter and hands back the |pref_store_contents| to the
  // initial caller of FilterOnLoad.
  virtual void FinalizeFilterOnLoad(
      PostFilterOnLoadCallback post_filter_on_load_callback,
      std::unique_ptr<base::DictionaryValue> pref_store_contents,
      bool prefs_altered) = 0;

  // Callback to be invoked only once (and subsequently reset) on the next
  // FilterOnLoad event. It will be allowed to modify the |prefs| handed to
  // FilterOnLoad before handing them back to this PrefHashFilter.
  FilterOnLoadInterceptor filter_on_load_interceptor_;
};

#endif  // SERVICES_PREFERENCES_TRACKED_INTERCEPTABLE_PREF_FILTER_H_
