// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/pref_store_client_mixin.h"

#include <utility>

#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "services/preferences/public/cpp/lib/util.h"
#include "services/preferences/public/cpp/pref_store_client.h"

namespace prefs {

template <typename BasePrefStore>
PrefStoreClientMixin<BasePrefStore>::PrefStoreClientMixin() = default;

template <typename BasePrefStore>
void PrefStoreClientMixin<BasePrefStore>::AddObserver(
    PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

template <typename BasePrefStore>
void PrefStoreClientMixin<BasePrefStore>::RemoveObserver(
    PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

template <typename BasePrefStore>
bool PrefStoreClientMixin<BasePrefStore>::HasObservers() const {
  return observers_.might_have_observers();
}

template <typename BasePrefStore>
bool PrefStoreClientMixin<BasePrefStore>::IsInitializationComplete() const {
  return initialized_ && static_cast<bool>(cached_prefs_);
}

template <typename BasePrefStore>
bool PrefStoreClientMixin<BasePrefStore>::GetValue(
    const std::string& key,
    const base::Value** result) const {
  DCHECK(initialized_);
  DCHECK(cached_prefs_);
  return cached_prefs_->Get(key, result);
}

template <typename BasePrefStore>
std::unique_ptr<base::DictionaryValue>
PrefStoreClientMixin<BasePrefStore>::GetValues() const {
  DCHECK(initialized_);
  DCHECK(cached_prefs_);
  return cached_prefs_->CreateDeepCopy();
}

template <typename BasePrefStore>
PrefStoreClientMixin<BasePrefStore>::~PrefStoreClientMixin() = default;

template <typename BasePrefStore>
void PrefStoreClientMixin<BasePrefStore>::Init(
    std::unique_ptr<base::DictionaryValue> initial_prefs,
    bool initialized,
    mojo::PendingReceiver<mojom::PrefStoreObserver> observer_receiver) {
  cached_prefs_ = std::move(initial_prefs);
  observer_receiver_.Bind(std::move(observer_receiver));
  if (initialized)
    OnInitializationCompleted(static_cast<bool>(cached_prefs_));
}

template <typename BasePrefStore>
base::DictionaryValue& PrefStoreClientMixin<BasePrefStore>::GetMutableValues() {
  DCHECK(cached_prefs_);
  return *cached_prefs_;
}

template <typename BasePrefStore>
void PrefStoreClientMixin<BasePrefStore>::ReportPrefValueChanged(
    const std::string& key) {
  for (auto& observer : observers_)
    observer.OnPrefValueChanged(key);
}

template <typename BasePrefStore>
void PrefStoreClientMixin<BasePrefStore>::OnPrefsChanged(
    std::vector<mojom::PrefUpdatePtr> updates) {
  for (const auto& update : updates)
    OnPrefChanged(update->key, std::move(update->value));
}

template <typename BasePrefStore>
void PrefStoreClientMixin<BasePrefStore>::OnInitializationCompleted(
    bool succeeded) {
  if (!initialized_) {
    initialized_ = true;
    for (auto& observer : observers_)
      observer.OnInitializationCompleted(succeeded);
  }
}

template <typename BasePrefStore>
void PrefStoreClientMixin<BasePrefStore>::OnPrefChangeAck() {}

template <typename BasePrefStore>
void PrefStoreClientMixin<BasePrefStore>::OnPrefChanged(
    const std::string& key,
    mojom::PrefUpdateValuePtr update_value) {
  DCHECK(cached_prefs_);
  bool changed = false;
  if (update_value->is_atomic_update()) {
    if (ShouldSkipWrite(key, std::vector<std::string>(),
                        OptionalOrNullptr(update_value->get_atomic_update()))) {
      return;
    }
    auto& optional = update_value->get_atomic_update();
    if (!optional.has_value()) {  // Delete
      if (cached_prefs_->RemovePath(key, nullptr))
        changed = true;
    } else {
      const base::Value* prev;
      if (cached_prefs_->Get(key, &prev)) {
        if (!prev->Equals(&optional.value())) {
          cached_prefs_->Set(
              key, base::Value::ToUniquePtrValue(std::move(optional.value())));
          changed = true;
        }
      } else {
        cached_prefs_->Set(
            key, base::Value::ToUniquePtrValue(std::move(optional.value())));
        changed = true;
      }
    }
  } else if (update_value->is_split_updates()) {
    auto& updates = update_value->get_split_updates();
    if (!updates.empty())
      changed = true;
    for (auto& update : updates) {
      // Clients shouldn't send empty paths.
      if (update->path.empty() ||
          ShouldSkipWrite(key, update->path,
                          OptionalOrNullptr(update->value))) {
        continue;
      }
      std::vector<base::StringPiece> full_path = base::SplitStringPiece(
          key, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      full_path.insert(full_path.end(), update->path.begin(),
                       update->path.end());
      prefs::SetValue(cached_prefs_.get(), full_path,
                      update->value ? base::Value::ToUniquePtrValue(
                                          std::move(*update->value))
                                    : nullptr);
    }
  }
  if (changed && initialized_)
    ReportPrefValueChanged(key);
}

template <typename BasePrefStore>
bool PrefStoreClientMixin<BasePrefStore>::ShouldSkipWrite(
    const std::string& key,
    const std::vector<std::string>& path,
    const base::Value* new_value) {
  return false;
}

template class PrefStoreClientMixin<::PrefStore>;
template class PrefStoreClientMixin<::PersistentPrefStore>;

}  // namespace prefs
