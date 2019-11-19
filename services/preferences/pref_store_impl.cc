// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/pref_store_impl.h"

#include <memory>
#include <set>
#include <utility>

#include "base/stl_util.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/preferences/public/cpp/lib/util.h"

namespace prefs {

class PrefStoreImpl::Observer {
 public:
  Observer(mojo::PendingRemote<mojom::PrefStoreObserver> observer,
           std::set<std::string> prefs)
      : observer_(std::move(observer)), prefs_(std::move(prefs)) {}

  void OnInitializationCompleted(bool succeeded) {
    observer_->OnInitializationCompleted(succeeded);
  }

  void OnPrefChanged(const std::string& key, const base::Value& value) const {
    if (!base::Contains(prefs_, key))
      return;

    std::vector<mojom::PrefUpdatePtr> updates;
    updates.push_back(mojom::PrefUpdate::New(
        key, mojom::PrefUpdateValue::NewAtomicUpdate(value.Clone()), 0));
    observer_->OnPrefsChanged(std::move(updates));
  }

  void OnPrefRemoved(const std::string& key) const {
    if (!base::Contains(prefs_, key))
      return;

    std::vector<mojom::PrefUpdatePtr> updates;
    updates.push_back(mojom::PrefUpdate::New(
        key, mojom::PrefUpdateValue::NewAtomicUpdate(base::nullopt), 0));
    observer_->OnPrefsChanged(std::move(updates));
  }

 private:
  mojo::Remote<mojom::PrefStoreObserver> observer_;
  const std::set<std::string> prefs_;

  DISALLOW_COPY_AND_ASSIGN(Observer);
};

PrefStoreImpl::PrefStoreImpl(scoped_refptr<::PrefStore> pref_store)
    : backing_pref_store_(std::move(pref_store)),
      backing_pref_store_initialized_(false) {
  DCHECK(backing_pref_store_);
  if (backing_pref_store_->IsInitializationComplete())
    OnInitializationCompleted(true);
  backing_pref_store_->AddObserver(this);
}

PrefStoreImpl::~PrefStoreImpl() {
  backing_pref_store_->RemoveObserver(this);
}

void PrefStoreImpl::OnPrefValueChanged(const std::string& key) {
  auto dictionary = std::make_unique<base::DictionaryValue>();
  const base::Value* value = nullptr;
  if (backing_pref_store_->GetValue(key, &value)) {
    for (const auto& observer : observers_)
      observer->OnPrefChanged(key, *value);
  } else {
    for (const auto& observer : observers_)
      observer->OnPrefRemoved(key);
  }
}

void PrefStoreImpl::OnInitializationCompleted(bool succeeded) {
  // Some pref stores call this more than once. We just ignore all calls after
  // the first.
  if (backing_pref_store_initialized_)
    DCHECK(succeeded);
  backing_pref_store_initialized_ = succeeded;
  for (const auto& observer : observers_)
    observer->OnInitializationCompleted(succeeded);
}

mojom::PrefStoreConnectionPtr PrefStoreImpl::AddObserver(
    const std::vector<std::string>& prefs_to_observe) {
  mojo::PendingRemote<mojom::PrefStoreObserver> observer_remote;
  std::set<std::string> observed_prefs(prefs_to_observe.begin(),
                                       prefs_to_observe.end());
  auto result = mojom::PrefStoreConnection::New(
      observer_remote.InitWithNewPipeAndPassReceiver(),
      base::Value::FromUniquePtrValue(
          FilterPrefs(backing_pref_store_->GetValues(), observed_prefs)),
      backing_pref_store_->IsInitializationComplete());
  observers_.push_back(std::make_unique<Observer>(std::move(observer_remote),
                                                  std::move(observed_prefs)));
  return result;
}

}  // namespace prefs
