// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/persistent_pref_store_impl.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "components/prefs/persistent_pref_store.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/preferences/public/cpp/lib/util.h"

namespace prefs {
namespace {

// Creates a PrefUpdateValuePtr representing |value| at |path|.
mojom::PrefUpdateValuePtr CreatePrefUpdate(const std::vector<std::string>& path,
                                           const base::Value* value) {
  if (path.empty()) {
    return mojom::PrefUpdateValue::NewAtomicUpdate(
        value ? base::make_optional(value->Clone()) : base::nullopt);
  }
  std::vector<mojom::SubPrefUpdatePtr> pref_updates;
  pref_updates.emplace_back(
      base::in_place, path,
      value ? base::make_optional(value->Clone()) : base::nullopt);
  return mojom::PrefUpdateValue::NewSplitUpdates(std::move(pref_updates));
}

// Returns a mojom::PrefUpdateValuePtr for |path| relative to |value|. If the
// full path does not exist, a PrefUpdateValue containing the closest value is
// returned.
//
// For example, for a |path| of {"foo", "bar"}:
//  - with a |value| of
//     {
//       "foo": 1
//     }
//   returns a path {"foo"} and value 1.
//
// - with a |value| of
//     {}
//   returns a path {"foo"} and null value.
//
// - with a |value| of
//     {
//       "foo": {}
//     }
//   returns a path {"foo", "bar"} and null value.
//
// - with a |value| of
//     {
//       "foo": {
//         "bar": "baz"
//       }
//     }
//   returns a path {"foo", "bar"} and value "baz".
mojom::PrefUpdateValuePtr LookupPrefUpdate(const std::vector<std::string>& path,
                                           const base::Value* value) {
  if (!value)
    return CreatePrefUpdate(std::vector<std::string>(), value);

  for (size_t i = 0; i < path.size(); ++i) {
    const base::DictionaryValue* dictionary_value = nullptr;
    if (!value->GetAsDictionary(&dictionary_value) ||
        !dictionary_value->Get(path[i], &value)) {
      return CreatePrefUpdate({path.begin(), path.begin() + i}, value);
    }
  }
  return CreatePrefUpdate(path, value);
}

}  // namespace

class PersistentPrefStoreImpl::Connection : public mojom::PersistentPrefStore {
 public:
  Connection(PersistentPrefStoreImpl* pref_store,
             mojo::PendingReceiver<mojom::PersistentPrefStore> receiver,
             mojo::PendingRemote<mojom::PrefStoreObserver> observer,
             ObservedPrefs observed_keys)
      : pref_store_(pref_store),
        receiver_(this, std::move(receiver)),
        observer_(std::move(observer)),
        observed_keys_(std::move(observed_keys)) {
    auto error_callback =
        base::Bind(&PersistentPrefStoreImpl::Connection::OnConnectionError,
                   base::Unretained(this));
    receiver_.set_disconnect_handler(error_callback);
    observer_.set_disconnect_handler(error_callback);
  }

  ~Connection() override = default;

  void OnPrefValuesChanged(const std::vector<mojom::PrefUpdatePtr>& updates) {
    if (write_in_progress_)
      return;

    std::vector<mojom::PrefUpdatePtr> filtered_updates;
    for (const auto& update : updates) {
      if (base::Contains(observed_keys_, update->key)) {
        filtered_updates.push_back(update->Clone());
      }
    }
    if (!filtered_updates.empty())
      observer_->OnPrefsChanged(std::move(filtered_updates));
  }

 private:
  // mojom::PersistentPrefStore:
  void SetValues(std::vector<mojom::PrefUpdatePtr> updates) override {
    base::AutoReset<bool> scoped_call_in_progress(&write_in_progress_, true);
    pref_store_->SetValues(std::move(updates));
    observer_->OnPrefChangeAck();
  }

  void RequestValue(const std::string& key,
                    const std::vector<std::string>& path) override {
    if (!base::Contains(observed_keys_, key))
      return;

    const base::Value* value = nullptr;
    pref_store_->GetValue(key, &value);
    std::vector<mojom::PrefUpdatePtr> updates;
    updates.emplace_back(base::in_place, key, LookupPrefUpdate(path, value), 0);
    observer_->OnPrefsChanged(std::move(updates));
  }

  void CommitPendingWrite(CommitPendingWriteCallback done_callback) override {
    // Note: PersistentPrefStore's synchronous callback part of the
    // CommitPendingWrite() API isn't supported on mojom::PersistentPrefStore at
    // the moment (see PersistentPrefStoreClient::CommitPendingWrite() for
    // details).
    pref_store_->CommitPendingWrite(std::move(done_callback));
  }
  void SchedulePendingLossyWrites() override {
    pref_store_->SchedulePendingLossyWrites();
  }
  void ClearMutableValues() override { pref_store_->ClearMutableValues(); }
  void OnStoreDeletionFromDisk() override {
    pref_store_->OnStoreDeletionFromDisk();
  }

  void OnConnectionError() { pref_store_->OnConnectionError(this); }

  // Owns |this|.
  PersistentPrefStoreImpl* const pref_store_;

  mojo::Receiver<mojom::PersistentPrefStore> receiver_;
  mojo::Remote<mojom::PrefStoreObserver> observer_;
  const ObservedPrefs observed_keys_;

  // If true then a write is in progress and any update notifications should be
  // ignored, as those updates would originate from ourselves.
  bool write_in_progress_ = false;

  DISALLOW_COPY_AND_ASSIGN(Connection);
};

PersistentPrefStoreImpl::PersistentPrefStoreImpl(
    scoped_refptr<PersistentPrefStore> backing_pref_store,
    base::OnceClosure on_initialized)
    : backing_pref_store_(backing_pref_store) {
  backing_pref_store_->AddObserver(this);
  if (!backing_pref_store_->IsInitializationComplete()) {
    on_initialized_ = std::move(on_initialized);
    initializing_ = true;
  }
}

PersistentPrefStoreImpl::~PersistentPrefStoreImpl() {
  backing_pref_store_->RemoveObserver(this);
}

mojom::PersistentPrefStoreConnectionPtr
PersistentPrefStoreImpl::CreateConnection(ObservedPrefs observed_prefs) {
  DCHECK(!initializing_);
  if (!backing_pref_store_->IsInitializationComplete()) {
    // |backing_pref_store_| initialization failed.
    return mojom::PersistentPrefStoreConnection::New(
        nullptr, mojo::NullRemote(), backing_pref_store_->GetReadError(),
        backing_pref_store_->ReadOnly());
  }
  mojo::PendingRemote<mojom::PersistentPrefStore> pref_store_remote;
  mojo::PendingRemote<mojom::PrefStoreObserver> observer;
  mojo::PendingReceiver<mojom::PrefStoreObserver> observer_receiver =
      observer.InitWithNewPipeAndPassReceiver();
  auto values = FilterPrefs(backing_pref_store_->GetValues(), observed_prefs);
  auto connection = std::make_unique<Connection>(
      this, pref_store_remote.InitWithNewPipeAndPassReceiver(),
      std::move(observer), std::move(observed_prefs));
  auto* connection_ptr = connection.get();
  connections_.insert(std::make_pair(connection_ptr, std::move(connection)));
  return mojom::PersistentPrefStoreConnection::New(
      mojom::PrefStoreConnection::New(std::move(observer_receiver),
                                      std::move(*values), true),
      std::move(pref_store_remote), backing_pref_store_->GetReadError(),
      backing_pref_store_->ReadOnly());
}

void PersistentPrefStoreImpl::OnPrefValueChanged(const std::string& key) {
  if (write_in_progress_)
    return;

  const base::Value* value = nullptr;
  for (auto& entry : connections_) {
    auto update_value = mojom::PrefUpdateValue::New();
    if (GetValue(key, &value)) {
      update_value->set_atomic_update(value->Clone());
    } else {
      update_value->set_atomic_update(base::nullopt);
    }
    std::vector<mojom::PrefUpdatePtr> updates;
    updates.emplace_back(base::in_place, key, std::move(update_value), 0);
    entry.first->OnPrefValuesChanged(updates);
  }
}

void PersistentPrefStoreImpl::OnInitializationCompleted(bool succeeded) {
  DCHECK(initializing_);
  initializing_ = false;
  std::move(on_initialized_).Run();
}

void PersistentPrefStoreImpl::SetValues(
    std::vector<mojom::PrefUpdatePtr> updates) {
  base::AutoReset<bool> scoped_call_in_progress(&write_in_progress_, true);
  for (auto& entry : connections_)
    entry.first->OnPrefValuesChanged(updates);

  for (auto& update : updates) {
    if (update->value->is_atomic_update()) {
      auto& value = update->value->get_atomic_update();
      if (value) {
        backing_pref_store_->SetValue(
            update->key, base::Value::ToUniquePtrValue(std::move(*value)),
            update->flags);
      } else {
        backing_pref_store_->RemoveValue(update->key, update->flags);
      }
    } else if (update->value->is_split_updates()) {
      base::Value* mutable_value = nullptr;
      base::DictionaryValue* dictionary_value = nullptr;
      std::unique_ptr<base::DictionaryValue> pending_dictionary;
      if (!backing_pref_store_->GetMutableValue(update->key, &mutable_value) ||
          !mutable_value->GetAsDictionary(&dictionary_value)) {
        pending_dictionary = std::make_unique<base::DictionaryValue>();
        dictionary_value = pending_dictionary.get();
      }
      std::set<std::vector<std::string>> updated_paths;
      for (auto& split_update : update->value->get_split_updates()) {
        if (split_update->path.empty())
          continue;

        SetValue(dictionary_value,
                 {split_update->path.begin(), split_update->path.end()},
                 split_update->value ? base::Value::ToUniquePtrValue(
                                           std::move(*split_update->value))
                                     : nullptr);
        updated_paths.insert(std::move(split_update->path));
      }
      if (pending_dictionary) {
        backing_pref_store_->SetValue(
            update->key, std::move(pending_dictionary), update->flags);
      } else {
        backing_pref_store_->ReportSubValuesChanged(
            update->key, std::move(updated_paths), update->flags);
      }
    }
  }
}

bool PersistentPrefStoreImpl::GetValue(const std::string& key,
                                       const base::Value** value) const {
  return backing_pref_store_->GetValue(key, value);
}

void PersistentPrefStoreImpl::CommitPendingWrite(
    base::OnceClosure done_callback) {
  backing_pref_store_->CommitPendingWrite(std::move(done_callback));
}

void PersistentPrefStoreImpl::SchedulePendingLossyWrites() {
  backing_pref_store_->SchedulePendingLossyWrites();
}

void PersistentPrefStoreImpl::ClearMutableValues() {
  backing_pref_store_->ClearMutableValues();
}

void PersistentPrefStoreImpl::OnStoreDeletionFromDisk() {
  backing_pref_store_->OnStoreDeletionFromDisk();
}

void PersistentPrefStoreImpl::OnConnectionError(Connection* connection) {
  connections_.erase(connection);
}

}  // namespace prefs
