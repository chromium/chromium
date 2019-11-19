// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/persistent_pref_store_client.h"

#include "base/bind.h"
#include "base/values.h"
#include "components/prefs/pref_registry.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "services/preferences/public/cpp/pref_registry_serializer.h"

namespace prefs {
namespace {

const base::Value* LookupPath(const base::Value* root,
                              const std::vector<std::string>& path_components) {
  const base::DictionaryValue* dictionary_value = nullptr;
  bool success = root->GetAsDictionary(&dictionary_value);
  DCHECK(success);

  for (size_t i = 0; i < path_components.size() - 1; ++i) {
    if (!dictionary_value->GetDictionaryWithoutPathExpansion(
            path_components[i], &dictionary_value)) {
      return nullptr;
    }
  }
  const base::Value* result = nullptr;
  dictionary_value->GetWithoutPathExpansion(path_components.back(), &result);
  return result;
}

template <typename StringType>
bool IsPrefix(const std::vector<StringType>& prefix,
              const std::vector<StringType>& full_path) {
  if (prefix.size() >= full_path.size())
    return false;

  for (size_t i = 0; i < prefix.size(); i++) {
    if (prefix[i] != full_path[i])
      return false;
  }
  return true;
}

// Removes paths that where a prefix is also contained in |updated_paths|.
void RemoveRedundantPaths(std::set<std::vector<std::string>>* updated_paths) {
  for (auto it = updated_paths->begin(), previous_it = updated_paths->end();
       it != updated_paths->end();) {
    if (previous_it != updated_paths->end() && IsPrefix(*previous_it, *it)) {
      it = updated_paths->erase(it);
    } else {
      previous_it = it;
      ++it;
    }
  }
}

}  // namespace

// A trie of writes that have been applied locally and sent to the service
// backend, but have not been acked.
class PersistentPrefStoreClient::InFlightWriteTrie {
 public:
  // Decision on what to do with writes incoming from the service.
  enum class Decision {
    // The write should be allowed.
    kAllow,

    // The write has already been superceded locally and should be ignored.
    kIgnore,

    // The write may have been partially superceded locally and should be
    // ignored but an updated value is needed from the service.
    kResolve
  };

  InFlightWriteTrie() = default;

  void Add() {
    std::vector<std::string> v;
    Add(v.begin(), v.end());
  }

  template <typename It, typename Jt>
  void Add(It path_start, Jt path_end) {
    if (path_start == path_end) {
      ++writes_in_flight_;
      return;
    }
    children_[*path_start].Add(path_start + 1, path_end);
  }

  bool Remove() {
    std::vector<std::string> v;
    return Remove(v.begin(), v.end());
  }

  template <typename It, typename Jt>
  bool Remove(It path_start, Jt path_end) {
    if (path_start == path_end) {
      DCHECK_GT(writes_in_flight_, 0);
      return --writes_in_flight_ == 0 && children_.empty();
    }
    auto it = children_.find(*path_start);
    DCHECK(it != children_.end()) << *path_start;
    auto removed = it->second.Remove(path_start + 1, path_end);
    if (removed)
      children_.erase(*path_start);

    return children_.empty() && writes_in_flight_ == 0;
  }

  template <typename It, typename Jt>
  Decision Lookup(It path_start, Jt path_end) {
    if (path_start == path_end) {
      if (children_.empty()) {
        DCHECK_GT(writes_in_flight_, 0);
        return Decision::kIgnore;
      }
      return Decision::kResolve;
    }

    if (writes_in_flight_ != 0) {
      return Decision::kIgnore;
    }
    auto it = children_.find(*path_start);
    if (it == children_.end()) {
      return Decision::kAllow;
    }

    return it->second.Lookup(path_start + 1, path_end);
  }

 private:
  std::map<std::string, InFlightWriteTrie> children_;
  int writes_in_flight_ = 0;

  DISALLOW_COPY_AND_ASSIGN(InFlightWriteTrie);
};

struct PersistentPrefStoreClient::InFlightWrite {
  std::string key;
  std::vector<std::vector<std::string>> sub_pref_paths;
};

PersistentPrefStoreClient::PersistentPrefStoreClient(
    mojom::PersistentPrefStoreConnectionPtr connection) {
  read_error_ = connection->read_error;
  read_only_ = connection->read_only;
  pref_store_.Bind(std::move(connection->pref_store));
  if (error_delegate_ && read_error_ != PREF_READ_ERROR_NONE)
    error_delegate_->OnError(read_error_);
  error_delegate_.reset();
  if (connection->pref_store_connection) {
    Init(base::DictionaryValue::From(base::Value::ToUniquePtrValue(
             std::move(connection->pref_store_connection->initial_prefs))),
         true, std::move(connection->pref_store_connection->observer));
  } else {
    Init(nullptr, false, mojo::NullReceiver());
  }
}

void PersistentPrefStoreClient::SetValue(const std::string& key,
                                         std::unique_ptr<base::Value> value,
                                         uint32_t flags) {
  base::Value* old_value = nullptr;
  GetMutableValues().Get(key, &old_value);
  if (!old_value || !value->Equals(old_value)) {
    GetMutableValues().Set(key, std::move(value));
    ReportValueChanged(key, flags);
  }
}

void PersistentPrefStoreClient::RemoveValue(const std::string& key,
                                            uint32_t flags) {
  if (GetMutableValues().RemovePath(key, nullptr))
    ReportValueChanged(key, flags);
}

bool PersistentPrefStoreClient::GetMutableValue(const std::string& key,
                                                base::Value** result) {
  return GetMutableValues().Get(key, result);
}

void PersistentPrefStoreClient::ReportValueChanged(const std::string& key,
                                                   uint32_t flags) {
  DCHECK(pref_store_);

  ReportSubValuesChanged(
      key, std::set<std::vector<std::string>>{std::vector<std::string>{}},
      flags);
}

void PersistentPrefStoreClient::ReportSubValuesChanged(
    const std::string& key,
    std::set<std::vector<std::string>> path_components,
    uint32_t flags) {
  QueueWrite(key, std::move(path_components), flags);
  ReportPrefValueChanged(key);
}

void PersistentPrefStoreClient::SetValueSilently(
    const std::string& key,
    std::unique_ptr<base::Value> value,
    uint32_t flags) {
  DCHECK(pref_store_);
  GetMutableValues().Set(key, std::move(value));
}

bool PersistentPrefStoreClient::ReadOnly() const {
  return read_only_;
}

PersistentPrefStore::PrefReadError PersistentPrefStoreClient::GetReadError()
    const {
  return read_error_;
}

PersistentPrefStore::PrefReadError PersistentPrefStoreClient::ReadPrefs() {
  return GetReadError();
}

void PersistentPrefStoreClient::ReadPrefsAsync(
    ReadErrorDelegate* error_delegate) {}

void PersistentPrefStoreClient::CommitPendingWrite(
    base::OnceClosure reply_callback,
    base::OnceClosure synchronous_done_callback) {
  // Supporting |synchronous_done_callback| semantics would require a sync IPC.
  // This isn't implemented as such at the moment as this functionality isn't
  // used in practice (if it ever becomes necessary, this check will fire).
  DCHECK(!synchronous_done_callback);

  DCHECK(pref_store_);
  if (!pending_writes_.empty())
    FlushPendingWrites();
  pref_store_->CommitPendingWrite(std::move(reply_callback));
}

void PersistentPrefStoreClient::SchedulePendingLossyWrites() {
  DCHECK(pref_store_);
  return pref_store_->SchedulePendingLossyWrites();
}

void PersistentPrefStoreClient::ClearMutableValues() {
  DCHECK(pref_store_);
  return pref_store_->ClearMutableValues();
}

void PersistentPrefStoreClient::OnStoreDeletionFromDisk() {
  DCHECK(pref_store_);
  return pref_store_->OnStoreDeletionFromDisk();
}

PersistentPrefStoreClient::~PersistentPrefStoreClient() {
  if (!pref_store_)
    return;

  CommitPendingWrite();
}

void PersistentPrefStoreClient::QueueWrite(
    const std::string& key,
    std::set<std::vector<std::string>> path_components,
    uint32_t flags) {
  DCHECK(!path_components.empty());
  if (pending_writes_.empty()) {
    // Use a weak pointer since a pending write should not prolong the life of
    // |this|. Instead, the destruction of |this| will flush any pending
    // writes.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&PersistentPrefStoreClient::FlushPendingWrites,
                       weak_factory_.GetWeakPtr()));
  }
  RemoveRedundantPaths(&path_components);
  auto& entry = pending_writes_[key];
  entry.second = flags;
  for (auto& path : path_components) {
    entry.first.insert(std::move(path));
  }
}

void PersistentPrefStoreClient::FlushPendingWrites() {
  weak_factory_.InvalidateWeakPtrs();
  if (pending_writes_.empty())
    return;

  std::vector<mojom::PrefUpdatePtr> updates;
  std::vector<InFlightWrite> writes;

  for (auto& pref : pending_writes_) {
    auto update_value = mojom::PrefUpdateValue::New();
    const base::Value* value = nullptr;
    if (GetValue(pref.first, &value)) {
      std::vector<mojom::SubPrefUpdatePtr> pref_updates;
      std::vector<std::vector<std::string>> sub_pref_writes;
      RemoveRedundantPaths(&pref.second.first);
      for (const auto& path : pref.second.first) {
        if (path.empty()) {
          pref_updates.clear();
          sub_pref_writes.clear();
          break;
        }
        const base::Value* nested_value = LookupPath(value, path);
        if (nested_value) {
          pref_updates.emplace_back(base::in_place, path,
                                    nested_value->Clone());
        } else {
          pref_updates.emplace_back(base::in_place, path, base::nullopt);
        }
        sub_pref_writes.push_back(path);
      }
      if (pref_updates.empty()) {
        update_value->set_atomic_update(value->Clone());
        writes.push_back({pref.first});
      } else {
        update_value->set_split_updates(std::move(pref_updates));
        writes.push_back({pref.first, std::move(sub_pref_writes)});
      }
    } else {
      update_value->set_atomic_update(base::nullopt);
      writes.push_back({pref.first});
    }
    updates.emplace_back(base::in_place, pref.first, std::move(update_value),
                         pref.second.second);
  }
  pref_store_->SetValues(std::move(updates));
  pending_writes_.clear();

  for (const auto& write : writes) {
    auto& trie = in_flight_writes_tries_[write.key];
    if (write.sub_pref_paths.empty()) {
      trie.Add();
    } else {
      for (const auto& subpref_update : write.sub_pref_paths) {
        trie.Add(subpref_update.begin(), subpref_update.end());
      }
    }
  }
  in_flight_writes_queue_.push(std::move(writes));
}

void PersistentPrefStoreClient::OnPrefChangeAck() {
  const auto& writes = in_flight_writes_queue_.front();
  for (const auto& write : writes) {
    auto it = in_flight_writes_tries_.find(write.key);
    DCHECK(it != in_flight_writes_tries_.end()) << write.key;
    bool remove = false;
    if (write.sub_pref_paths.empty()) {
      remove = it->second.Remove();
    } else {
      for (const auto& subpref_update : write.sub_pref_paths) {
        remove =
            it->second.Remove(subpref_update.begin(), subpref_update.end());
      }
    }
    if (remove) {
      in_flight_writes_tries_.erase(it);
    }
  }
  in_flight_writes_queue_.pop();
}

bool PersistentPrefStoreClient::ShouldSkipWrite(
    const std::string& key,
    const std::vector<std::string>& path,
    const base::Value* new_value) {
  if (!pending_writes_.empty()) {
    FlushPendingWrites();
  }
  auto it = in_flight_writes_tries_.find(key);
  if (it == in_flight_writes_tries_.end()) {
    return false;
  }

  auto decision = it->second.Lookup(path.begin(), path.end());
  if (decision == InFlightWriteTrie::Decision::kAllow) {
    return false;
  }
  if (decision == InFlightWriteTrie::Decision::kResolve)
    pref_store_->RequestValue(key, path);
  return true;
}

}  // namespace prefs
