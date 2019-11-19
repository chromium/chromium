// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_PUBLIC_CPP_PERSISTENT_PREF_STORE_CLIENT_H_
#define SERVICES_PREFERENCES_PUBLIC_CPP_PERSISTENT_PREF_STORE_CLIENT_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_value_store.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/preferences/public/cpp/pref_store_client_mixin.h"
#include "services/preferences/public/mojom/preferences.mojom.h"

namespace base {
class Value;
}

namespace prefs {

// An implementation of PersistentPrefStore backed by a
// mojom::PersistentPrefStore and a mojom::PrefStoreObserver.
class PersistentPrefStoreClient
    : public PrefStoreClientMixin<PersistentPrefStore> {
 public:
  explicit PersistentPrefStoreClient(
      mojom::PersistentPrefStoreConnectionPtr connection);

  // WriteablePrefStore:
  void SetValue(const std::string& key,
                std::unique_ptr<base::Value> value,
                uint32_t flags) override;
  void RemoveValue(const std::string& key, uint32_t flags) override;
  bool GetMutableValue(const std::string& key, base::Value** result) override;
  void ReportValueChanged(const std::string& key, uint32_t flags) override;
  void ReportSubValuesChanged(
      const std::string& key,
      std::set<std::vector<std::string>> path_components,
      uint32_t flags) override;
  void SetValueSilently(const std::string& key,
                        std::unique_ptr<base::Value> value,
                        uint32_t flags) override;

  // PersistentPrefStore:
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

 protected:
  // base::RefCounted<PrefStore>:
  ~PersistentPrefStoreClient() override;

 private:
  class InFlightWriteTrie;
  struct InFlightWrite;

  void QueueWrite(const std::string& key,
                  std::set<std::vector<std::string>> path_components,
                  uint32_t flags);
  void FlushPendingWrites();

  // prefs::mojom::PreferenceObserver:
  void OnPrefChangeAck() override;

  bool ShouldSkipWrite(const std::string& key,
                       const std::vector<std::string>& path,
                       const base::Value* new_value) override;

  bool read_only_ = false;
  PrefReadError read_error_ = PersistentPrefStore::PREF_READ_ERROR_NONE;
  mojo::Remote<mojom::PersistentPrefStore> pref_store_;
  std::map<std::string, std::pair<std::set<std::vector<std::string>>, uint32_t>>
      pending_writes_;

  std::unique_ptr<ReadErrorDelegate> error_delegate_;

  base::queue<std::vector<InFlightWrite>> in_flight_writes_queue_;
  std::map<std::string, InFlightWriteTrie> in_flight_writes_tries_;

  base::WeakPtrFactory<PersistentPrefStoreClient> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PersistentPrefStoreClient);
};

}  // namespace prefs

#endif  // SERVICES_PREFERENCES_PUBLIC_CPP_PERSISTENT_PREF_STORE_CLIENT_H_
