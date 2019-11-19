// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_PREF_STORE_IMPL_H_
#define SERVICES_PREFERENCES_PREF_STORE_IMPL_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/prefs/pref_store.h"
#include "components/prefs/pref_value_store.h"
#include "services/preferences/public/mojom/preferences.mojom.h"

namespace prefs {

// Wraps an actual PrefStore implementation and exposes it as a
// mojom::PrefStore interface.
class PrefStoreImpl : public ::PrefStore::Observer {
 public:
  explicit PrefStoreImpl(scoped_refptr<::PrefStore> pref_store);
  ~PrefStoreImpl() override;

  mojom::PrefStoreConnectionPtr AddObserver(
      const std::vector<std::string>& prefs_to_observe);

 private:
  class Observer;

  // PrefStore::Observer:
  void OnPrefValueChanged(const std::string& key) override;
  void OnInitializationCompleted(bool succeeded) override;

  // The backing store we observer for changes.
  scoped_refptr<::PrefStore> backing_pref_store_;

  // Observers we notify when |backing_pref_store_| changes.
  std::vector<std::unique_ptr<Observer>> observers_;

  // True when the |backing_pref_store_| is initialized, either because it was
  // passed already initialized in the constructor or after
  // OnInitializationCompleted was called.
  bool backing_pref_store_initialized_;

  DISALLOW_COPY_AND_ASSIGN(PrefStoreImpl);
};

}  // namespace prefs

#endif  // SERVICES_PREFERENCES_PREF_STORE_IMPL_H_
