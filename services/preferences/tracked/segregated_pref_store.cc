// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/segregated_pref_store.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/values.h"

SegregatedPrefStore::AggregatingObserver::AggregatingObserver(
    SegregatedPrefStore* outer)
    : outer_(outer),
      failed_sub_initializations_(0),
      successful_sub_initializations_(0) {}

void SegregatedPrefStore::AggregatingObserver::OnPrefValueChanged(
    const std::string& key) {
  // There is no need to tell clients about changes if they have not yet been
  // told about initialization.
  if (failed_sub_initializations_ + successful_sub_initializations_ < 2)
    return;

  for (auto& observer : outer_->observers_)
    observer.OnPrefValueChanged(key);
}

void SegregatedPrefStore::AggregatingObserver::OnInitializationCompleted(
    bool succeeded) {
  if (succeeded)
    ++successful_sub_initializations_;
  else
    ++failed_sub_initializations_;

  DCHECK_LE(failed_sub_initializations_ + successful_sub_initializations_, 2);

  if (failed_sub_initializations_ + successful_sub_initializations_ == 2) {
    if (successful_sub_initializations_ == 2 && outer_->read_error_delegate_) {
      PersistentPrefStore::PrefReadError read_error = outer_->GetReadError();
      if (read_error != PersistentPrefStore::PREF_READ_ERROR_NONE)
        outer_->read_error_delegate_->OnError(read_error);
    }

    for (auto& observer : outer_->observers_)
      observer.OnInitializationCompleted(successful_sub_initializations_ == 2);
  }
}

SegregatedPrefStore::SegregatedPrefStore(
    const scoped_refptr<PersistentPrefStore>& default_pref_store,
    const scoped_refptr<PersistentPrefStore>& selected_pref_store,
    const std::set<std::string>& selected_pref_names,
    mojo::Remote<prefs::mojom::TrackedPreferenceValidationDelegate>
        validation_delegate)
    : validation_delegate_(std::move(validation_delegate)),
      default_pref_store_(default_pref_store),
      selected_pref_store_(selected_pref_store),
      selected_preference_names_(selected_pref_names),
      aggregating_observer_(this) {
  default_pref_store_->AddObserver(&aggregating_observer_);
  selected_pref_store_->AddObserver(&aggregating_observer_);
}

void SegregatedPrefStore::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SegregatedPrefStore::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool SegregatedPrefStore::HasObservers() const {
  return observers_.might_have_observers();
}

bool SegregatedPrefStore::IsInitializationComplete() const {
  return default_pref_store_->IsInitializationComplete() &&
         selected_pref_store_->IsInitializationComplete();
}

bool SegregatedPrefStore::GetValue(const std::string& key,
                                   const base::Value** result) const {
  return StoreForKey(key)->GetValue(key, result);
}

std::unique_ptr<base::DictionaryValue> SegregatedPrefStore::GetValues() const {
  auto values = default_pref_store_->GetValues();
  auto selected_pref_store_values = selected_pref_store_->GetValues();
  for (const auto& key : selected_preference_names_) {
    const base::Value* value = nullptr;
    if (selected_pref_store_values->Get(key, &value)) {
      values->Set(key, value->CreateDeepCopy());
    } else {
      values->Remove(key, nullptr);
    }
  }
  return values;
}

void SegregatedPrefStore::SetValue(const std::string& key,
                                   std::unique_ptr<base::Value> value,
                                   uint32_t flags) {
  StoreForKey(key)->SetValue(key, std::move(value), flags);
}

void SegregatedPrefStore::RemoveValue(const std::string& key, uint32_t flags) {
  StoreForKey(key)->RemoveValue(key, flags);
}

bool SegregatedPrefStore::GetMutableValue(const std::string& key,
                                          base::Value** result) {
  return StoreForKey(key)->GetMutableValue(key, result);
}

void SegregatedPrefStore::ReportValueChanged(const std::string& key,
                                             uint32_t flags) {
  StoreForKey(key)->ReportValueChanged(key, flags);
}

void SegregatedPrefStore::SetValueSilently(const std::string& key,
                                           std::unique_ptr<base::Value> value,
                                           uint32_t flags) {
  StoreForKey(key)->SetValueSilently(key, std::move(value), flags);
}

bool SegregatedPrefStore::ReadOnly() const {
  return selected_pref_store_->ReadOnly() || default_pref_store_->ReadOnly();
}

PersistentPrefStore::PrefReadError SegregatedPrefStore::GetReadError() const {
  PersistentPrefStore::PrefReadError read_error =
      default_pref_store_->GetReadError();
  if (read_error == PersistentPrefStore::PREF_READ_ERROR_NONE) {
    read_error = selected_pref_store_->GetReadError();
    // Ignore NO_FILE from selected_pref_store_.
    if (read_error == PersistentPrefStore::PREF_READ_ERROR_NO_FILE)
      read_error = PersistentPrefStore::PREF_READ_ERROR_NONE;
  }
  return read_error;
}

PersistentPrefStore::PrefReadError SegregatedPrefStore::ReadPrefs() {
  // Note: Both of these stores own PrefFilters which makes ReadPrefs
  // asynchronous. This is okay in this case as only the first call will be
  // truly asynchronous, the second call will then unblock the migration in
  // TrackedPreferencesMigrator and complete synchronously.
  default_pref_store_->ReadPrefs();
  PersistentPrefStore::PrefReadError selected_store_read_error =
      selected_pref_store_->ReadPrefs();
  DCHECK_NE(PersistentPrefStore::PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE,
            selected_store_read_error);

  return GetReadError();
}

void SegregatedPrefStore::ReadPrefsAsync(ReadErrorDelegate* error_delegate) {
  read_error_delegate_.reset(error_delegate);
  default_pref_store_->ReadPrefsAsync(NULL);
  selected_pref_store_->ReadPrefsAsync(NULL);
}

void SegregatedPrefStore::CommitPendingWrite(
    base::OnceClosure reply_callback,
    base::OnceClosure synchronous_done_callback) {
  // A BarrierClosure will run its callback wherever the last instance of the
  // returned wrapper is invoked. As such it is guaranteed to respect the reply
  // vs synchronous semantics assuming |default_pref_store_| and
  // |selected_pref_store_| honor it.

  base::RepeatingClosure reply_callback_wrapper =
      reply_callback ? base::BarrierClosure(2, std::move(reply_callback))
                     : base::RepeatingClosure();

  base::RepeatingClosure synchronous_callback_wrapper =
      synchronous_done_callback
          ? base::BarrierClosure(2, std::move(synchronous_done_callback))
          : base::RepeatingClosure();

  default_pref_store_->CommitPendingWrite(reply_callback_wrapper,
                                          synchronous_callback_wrapper);
  selected_pref_store_->CommitPendingWrite(reply_callback_wrapper,
                                           synchronous_callback_wrapper);
}

void SegregatedPrefStore::SchedulePendingLossyWrites() {
  default_pref_store_->SchedulePendingLossyWrites();
  selected_pref_store_->SchedulePendingLossyWrites();
}

void SegregatedPrefStore::ClearMutableValues() {
  NOTIMPLEMENTED();
}

void SegregatedPrefStore::OnStoreDeletionFromDisk() {
  default_pref_store_->OnStoreDeletionFromDisk();
  selected_pref_store_->OnStoreDeletionFromDisk();
}

SegregatedPrefStore::~SegregatedPrefStore() {
  default_pref_store_->RemoveObserver(&aggregating_observer_);
  selected_pref_store_->RemoveObserver(&aggregating_observer_);
}

PersistentPrefStore* SegregatedPrefStore::StoreForKey(const std::string& key) {
  return (base::Contains(selected_preference_names_, key) ? selected_pref_store_
                                                          : default_pref_store_)
      .get();
}

const PersistentPrefStore* SegregatedPrefStore::StoreForKey(
    const std::string& key) const {
  return (base::Contains(selected_preference_names_, key) ? selected_pref_store_
                                                          : default_pref_store_)
      .get();
}
