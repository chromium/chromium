// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_SEGREGATED_PREF_STORE_H_
#define SERVICES_PREFERENCES_TRACKED_SEGREGATED_PREF_STORE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "components/prefs/persistent_pref_store.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"

// Provides a unified PersistentPrefStore implementation that splits its storage
// and retrieval between two underlying PersistentPrefStore instances: a set of
// preference names is used to partition the preferences.
//
// Combines properties of the two stores as follows:
//   * The unified read error will be:
//                           Selected Store Error
//    Default Store Error | NO_ERROR      | NO_FILE       | other selected |
//               NO_ERROR | NO_ERROR      | NO_ERROR      | other selected |
//               NO_FILE  | NO_FILE       | NO_FILE       | NO_FILE        |
//          other default | other default | other default | other default  |
//   * The unified initialization success, initialization completion, and
//     read-only state are the boolean OR of the underlying stores' properties.
class SegregatedPrefStore : public PersistentPrefStore {
 public:
  // Creates an instance that delegates to |selected_pref_store| for the
  // preferences named in |selected_pref_names| and to |default_pref_store|
  // for all others. If an unselected preference is present in
  // |selected_pref_store| (i.e. because it was previously selected) it will
  // be migrated back to |default_pref_store| upon access via a non-const
  // method.
  // |on_initialization| will be invoked when both stores have been initialized,
  // before observers of the SegregatedPrefStore store are notified.
  SegregatedPrefStore(
      const scoped_refptr<PersistentPrefStore>& default_pref_store,
      const scoped_refptr<PersistentPrefStore>& selected_pref_store,
      const std::set<std::string>& selected_pref_names,
      mojo::Remote<prefs::mojom::TrackedPreferenceValidationDelegate>
          validation_delegate);

  // PrefStore implementation
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObservers() const override;
  bool IsInitializationComplete() const override;
  bool GetValue(const std::string& key,
                const base::Value** result) const override;
  std::unique_ptr<base::DictionaryValue> GetValues() const override;

  // WriteablePrefStore implementation
  void SetValue(const std::string& key,
                std::unique_ptr<base::Value> value,
                uint32_t flags) override;
  void RemoveValue(const std::string& key, uint32_t flags) override;

  // PersistentPrefStore implementation
  bool GetMutableValue(const std::string& key, base::Value** result) override;
  void ReportValueChanged(const std::string& key, uint32_t flags) override;
  void SetValueSilently(const std::string& key,
                        std::unique_ptr<base::Value> value,
                        uint32_t flags) override;
  bool ReadOnly() const override;
  PrefReadError GetReadError() const override;
  PrefReadError ReadPrefs() override;
  void ReadPrefsAsync(ReadErrorDelegate* error_delegate) override;
  void CommitPendingWrite(
      base::OnceClosure reply_callback = base::OnceClosure(),
      base::OnceClosure synchronous_done_callback =
          base::OnceClosure()) override;
  void SchedulePendingLossyWrites() override;
  void ClearMutableValues() override;
  void OnStoreDeletionFromDisk() override;

 private:
  // Aggregates events from the underlying stores and synthesizes external
  // events via |on_initialization|, |read_error_delegate_|, and |observers_|.
  class AggregatingObserver : public PrefStore::Observer {
   public:
    explicit AggregatingObserver(SegregatedPrefStore* outer);

    // PrefStore::Observer implementation
    void OnPrefValueChanged(const std::string& key) override;
    void OnInitializationCompleted(bool succeeded) override;

   private:
    SegregatedPrefStore* outer_;
    int failed_sub_initializations_;
    int successful_sub_initializations_;

    DISALLOW_COPY_AND_ASSIGN(AggregatingObserver);
  };

  ~SegregatedPrefStore() override;

  // Returns |selected_pref_store| if |key| is selected and |default_pref_store|
  // otherwise.
  PersistentPrefStore* StoreForKey(const std::string& key);
  const PersistentPrefStore* StoreForKey(const std::string& key) const;

  // |validation_delegate_| is used by |default_pref_store_| and
  // |selected_pref_store_| PrefHashFilters. Its lifetime is managed here since
  // a single owner is required.
  mojo::Remote<prefs::mojom::TrackedPreferenceValidationDelegate>
      validation_delegate_;

  scoped_refptr<PersistentPrefStore> default_pref_store_;
  scoped_refptr<PersistentPrefStore> selected_pref_store_;
  std::set<std::string> selected_preference_names_;

  std::unique_ptr<PersistentPrefStore::ReadErrorDelegate> read_error_delegate_;
  base::ObserverList<PrefStore::Observer, true>::Unchecked observers_;
  AggregatingObserver aggregating_observer_;

  DISALLOW_COPY_AND_ASSIGN(SegregatedPrefStore);
};

#endif  // SERVICES_PREFERENCES_TRACKED_SEGREGATED_PREF_STORE_H_
