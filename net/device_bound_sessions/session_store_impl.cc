// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_store_impl.h"

#include <algorithm>

#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "net/base/schemeful_site.h"
#include "net/device_bound_sessions/proto/storage.pb.h"

namespace net::device_bound_sessions {

namespace {

using unexportable_keys::BackgroundTaskPriority;
using unexportable_keys::ServiceError;
using unexportable_keys::ServiceErrorOr;
using unexportable_keys::UnexportableKeyId;
using unexportable_keys::UnexportableKeyService;

// Priority is set to `USER_VISIBLE` because the initial load of
// sessions from disk is required to complete before URL requests
// can be checked to see if they are associated with bound sessions.
constexpr base::TaskTraits kDBTaskTraits = {
    base::MayBlock(), base::TaskPriority::USER_VISIBLE,
    base::TaskShutdownBehavior::BLOCK_SHUTDOWN};

const int kCurrentSchemaVersion = 1;
const char kSessionTableName[] = "dbsc_session_tbl";
const base::TimeDelta kFlushDelay = base::Seconds(2);

SessionStoreImpl::DBStatus InitializeOnDbSequence(
    sql::Database* db,
    base::FilePath db_storage_path,
    sqlite_proto::ProtoTableManager* table_manager,
    sqlite_proto::KeyValueData<proto::SiteSessions>* session_data) {
  if (db->Open(db_storage_path) == false) {
    return SessionStoreImpl::DBStatus::kFailure;
  }

  db->Preload();

  table_manager->InitializeOnDbSequence(
      db, std::vector<std::string>{kSessionTableName}, kCurrentSchemaVersion);
  session_data->InitializeOnDBSequence();

  return SessionStoreImpl::DBStatus::kSuccess;
}

}  // namespace

SessionStoreImpl::SessionStoreImpl(base::FilePath db_storage_path,
                                   UnexportableKeyService& key_service)
    : key_service_(key_service),
      db_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(kDBTaskTraits)),
      db_storage_path_(std::move(db_storage_path)),
      db_(std::make_unique<sql::Database>(
          sql::DatabaseOptions{.page_size = 4096, .cache_size = 500})),
      table_manager_(base::MakeRefCounted<sqlite_proto::ProtoTableManager>(
          db_task_runner_)),
      session_table_(
          std::make_unique<sqlite_proto::KeyValueTable<proto::SiteSessions>>(
              kSessionTableName)),
      session_data_(
          std::make_unique<sqlite_proto::KeyValueData<proto::SiteSessions>>(
              table_manager_,
              session_table_.get(),
              /*max_num_entries=*/std::nullopt,
              kFlushDelay)) {
  db_->set_histogram_tag("DBSCSessions");
}

SessionStoreImpl::~SessionStoreImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (db_status_ == DBStatus::kSuccess) {
    session_data_->FlushDataToDisk();
  }

  // Shutdown `table_manager_`, and delete it together with `db_`
  // and KeyValueTable on DB sequence, then delete the KeyValueData
  // and call `shutdown_callback_` on main sequence.
  // This ensures that DB objects outlive any other task posted to DB
  // sequence, since their deletion is the very last posted task.
  db_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<sqlite_proto::ProtoTableManager> table_manager,
             std::unique_ptr<sql::Database> db,
             auto session_table) { table_manager->WillShutdown(); },
          std::move(table_manager_), std::move(db_), std::move(session_table_)),
      base::BindOnce(
          [](auto session_data, base::OnceClosure shutdown_callback) {
            if (shutdown_callback) {
              std::move(shutdown_callback).Run();
            }
          },
          std::move(session_data_), std::move(shutdown_callback_)));
}

void SessionStoreImpl::LoadSessions(LoadSessionsCallback callback) {
  CHECK_EQ(db_status_, DBStatus::kNotLoaded);

  // This is safe because tasks are serialized on the db_task_runner sequence
  // and the `table_manager_` and `session_data_` are only freed after a
  // response from a task (triggered by the destructor) runs on the
  // `db_task_runner_`.
  // Similarly, the `db_` is not actually destroyed until the task
  // triggered by the destructor runs on the `db_task_runner_`.
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&InitializeOnDbSequence, base::Unretained(db_.get()),
                     db_storage_path_, base::Unretained(table_manager_.get()),
                     base::Unretained(session_data_.get())),
      base::BindOnce(&SessionStoreImpl::OnDatabaseLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SessionStoreImpl::OnDatabaseLoaded(LoadSessionsCallback callback,
                                        DBStatus db_status) {
  db_status_ = db_status;
  SessionsMap sessions;
  if (db_status == DBStatus::kSuccess) {
    std::vector<std::string> keys_to_delete;
    sessions = CreateSessionsFromLoadedData(session_data_->GetAllCached(),
                                            keys_to_delete);
    if (keys_to_delete.size() > 0) {
      session_data_->DeleteData(keys_to_delete);
    }
  }
  std::move(callback).Run(std::move(sessions));
}

// static
SessionStore::SessionsMap SessionStoreImpl::CreateSessionsFromLoadedData(
    const std::map<std::string, proto::SiteSessions>& loaded_data,
    std::vector<std::string>& keys_to_delete) {
  SessionsMap all_sessions;
  for (const auto& [site_str, site_proto] : loaded_data) {
    SchemefulSite site = net::SchemefulSite::Deserialize(site_str);
    if (site.opaque()) {
      keys_to_delete.push_back(site_str);
      continue;
    }

    bool invalid_session_found = false;
    SessionsMap site_sessions;
    for (const auto& [session_id, session_proto] : site_proto.sessions()) {
      if (!session_proto.has_wrapped_key() ||
          session_proto.wrapped_key().empty()) {
        invalid_session_found = true;
        break;
      }

      std::unique_ptr<Session> session =
          Session::CreateFromProto(session_proto);
      if (!session) {
        invalid_session_found = true;
        break;
      }

      // Restored session entry has passed basic validation checks. Save it.
      site_sessions.emplace(site, std::move(session));
    }

    // Remove the entire site entry from the DB if a single invalid session is
    // found as it could be a sign of data corruption or external manipulation.
    // Note: A session could also cease to be valid because the criteria for
    // validity changed after a Chrome update. In this scenario, however, we
    // would migrate that session rather than deleting the site sessions.
    if (invalid_session_found) {
      keys_to_delete.push_back(site_str);
    } else {
      all_sessions.merge(site_sessions);
    }
  }

  return all_sessions;
}

void SessionStoreImpl::SetShutdownCallbackForTesting(
    base::OnceClosure shutdown_callback) {
  shutdown_callback_ = std::move(shutdown_callback);
}

void SessionStoreImpl::SaveSession(const SchemefulSite& site,
                                   const Session& session) {
  if (db_status_ != DBStatus::kSuccess) {
    return;
  }

  CHECK(session.unexportable_key_id().has_value());

  // Wrap the unexportable key into a persistable form.
  ServiceErrorOr<std::vector<uint8_t>> wrapped_key =
      key_service_->GetWrappedKey(*session.unexportable_key_id());
  // Don't bother persisting the session if wrapping fails because we will throw
  // away all persisted data if the wrapped key is missing for any session.
  if (!wrapped_key.has_value()) {
    return;
  }

  proto::Session session_proto = session.ToProto();
  session_proto.set_wrapped_key(
      std::string(wrapped_key->begin(), wrapped_key->end()));
  proto::SiteSessions site_proto;
  std::string site_str = site.Serialize();
  session_data_->TryGetData(site_str, &site_proto);
  (*site_proto.mutable_sessions())[session_proto.id()] =
      std::move(session_proto);

  session_data_->UpdateData(site_str, site_proto);
}

void SessionStoreImpl::DeleteSession(const SchemefulSite& site,
                                     const Session::Id& session_id) {
  if (db_status_ != DBStatus::kSuccess) {
    return;
  }

  proto::SiteSessions site_proto;
  std::string site_str = site.Serialize();
  if (!session_data_->TryGetData(site_str, &site_proto)) {
    return;
  }

  if (site_proto.sessions().count(*session_id) == 0) {
    return;
  }

  // If this is the only session associated with the site,
  // delete the site entry.
  if (site_proto.mutable_sessions()->size() == 1) {
    session_data_->DeleteData({site_str});
    return;
  }

  site_proto.mutable_sessions()->erase(*session_id);

  // Schedule a DB update for the site entry.
  session_data_->UpdateData(site.Serialize(), site_proto);
}

SessionStore::SessionsMap SessionStoreImpl::GetAllSessions() const {
  if (db_status_ != DBStatus::kSuccess) {
    return SessionsMap();
  }

  std::vector<std::string> keys_to_delete;
  SessionsMap all_sessions = CreateSessionsFromLoadedData(
      session_data_->GetAllCached(), keys_to_delete);
  // We shouldn't find invalid keys at this point, they should have all been
  // filtered out in the `LoadSessions` operations.
  CHECK(keys_to_delete.empty());

  return all_sessions;
}

void SessionStoreImpl::RestoreSessionBindingKey(
    const SchemefulSite& site,
    const Session::Id& session_id,
    RestoreSessionBindingKeyCallback callback) {
  auto key_id_or_error = base::unexpected(ServiceError::kKeyNotFound);
  if (db_status_ != DBStatus::kSuccess) {
    std::move(callback).Run(key_id_or_error);
    return;
  }

  // Retrieve the session's persisted binding key and unwrap it.
  proto::SiteSessions site_proto;
  if (session_data_->TryGetData(site.Serialize(), &site_proto)) {
    auto it = site_proto.sessions().find(*session_id);
    if (it != site_proto.sessions().end()) {
      // Unwrap the binding key asynchronously.
      std::vector<uint8_t> wrapped_key(it->second.wrapped_key().begin(),
                                       it->second.wrapped_key().end());
      key_service_->FromWrappedSigningKeySlowlyAsync(
          wrapped_key, BackgroundTaskPriority::kUserVisible,
          std::move(callback));
      return;
    }
  }

  // The session is not present in the store,
  // invoke the callback immediately.
  std::move(callback).Run(key_id_or_error);
}

}  // namespace net::device_bound_sessions
