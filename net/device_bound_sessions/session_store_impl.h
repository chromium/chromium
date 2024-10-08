// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_STORE_IMPL_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_STORE_IMPL_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "components/sqlite_proto/key_value_data.h"
#include "components/sqlite_proto/key_value_table.h"
#include "components/sqlite_proto/proto_table_manager.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "net/base/net_export.h"
#include "net/device_bound_sessions/proto/storage.pb.h"
#include "net/device_bound_sessions/session_store.h"

namespace unexportable_keys {
class UnexportableKeyService;
}

namespace net::device_bound_sessions {

// `SessionStoreImpl` implements a persistent store for sessions data.
// It uses the sqlite-proto library to store the data in a string-to-proto
// SQLite table. The key is a serialized `SchemefulSite` string that
// represents an eTLD+1 site. The value is a protobuf of session objects
// associated the site.
class NET_EXPORT SessionStoreImpl : public SessionStore {
 public:
  enum DBStatus {
    kSuccess,
    kFailure,
    kNotLoaded,
  };

  // Instantiates a store object.
  // `db_storage_path` is the path to the underlying SQLite DB file.
  // `key_service` is used to convert a session binding key to/from
  // its persistable form.
  SessionStoreImpl(base::FilePath db_storage_path,
                   unexportable_keys::UnexportableKeyService& key_service);

  SessionStoreImpl(const SessionStoreImpl& other) = delete;
  SessionStoreImpl& operator=(const SessionStoreImpl& other) = delete;
  SessionStoreImpl(SessionStoreImpl&& other) = delete;
  SessionStoreImpl& operator=(SessionStoreImpl&& other) = delete;

  ~SessionStoreImpl() override;

  // SessionStore implementation:
  void LoadSessions(LoadSessionsCallback callback) override;
  void SaveSession(const SchemefulSite& site, const Session& session) override;
  void DeleteSession(const SchemefulSite& site,
                     const Session::Id& session_id) override;
  SessionsMap GetAllSessions() const override;
  void RestoreSessionBindingKey(
      const SchemefulSite& site,
      const Session::Id& session_id,
      RestoreSessionBindingKeyCallback callback) override;

  DBStatus db_status() const { return db_status_; }

  // Allows test to wait until DB shutdown tasks are complete.
  void SetShutdownCallbackForTesting(base::OnceClosure shutdown_callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(SessionStoreImplTest,
                           PruneLoadedEntryWithInvalidSite);
  FRIEND_TEST_ALL_PREFIXES(SessionStoreImplTest,
                           PruneLoadedEntryWithInvalidSession);
  FRIEND_TEST_ALL_PREFIXES(SessionStoreImplTest,
                           PruneLoadedEntryWithSessionMissingWrappedKey);

  void OnDatabaseLoaded(LoadSessionsCallback callback, DBStatus status);

  // Helper function called by `OnDatabaseLoaded` to prune out any invalid
  // entries found in the data loaded from disk. Returns a map of valid
  // session objects that is returned to the caller of `LoadSessions` via
  // the provided callback. `keys_to_delete` represents the list of invalid
  // keys that are deleted from the store.
  static SessionsMap CreateSessionsFromLoadedData(
      const std::map<std::string, proto::SiteSessions>& loaded_data,
      std::vector<std::string>& keys_to_delete);

  // Key service used to wrap/unwrap unexportable session keys.
  const raw_ref<unexportable_keys::UnexportableKeyService> key_service_;

  // Background task runner used to perform DB tasks.
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
  // Path to the backing database file.
  base::FilePath db_storage_path_;

  // The following objects are use to work with an SQLite database.
  //`db_`, `proto_table_manager_` are deleted on the db sequence,
  // while `session_table_` and `session_data_` are deleted on the
  // main sequence.
  std::unique_ptr<sql::Database> db_;
  scoped_refptr<sqlite_proto::ProtoTableManager> table_manager_;
  std::unique_ptr<sqlite_proto::KeyValueTable<proto::SiteSessions>>
      session_table_;
  // TODO(crbug.com/371556007) : Keeping the `session_data_` around
  // facilitates DB operations that would otherwise require read+write
  // operations. However, it does create some redundancy in the cached
  // data since we also convert the cached data into `Session` objects.
  // Look into reducing the cached data storage size.
  std::unique_ptr<sqlite_proto::KeyValueData<proto::SiteSessions>>
      session_data_;

  DBStatus db_status_ = kNotLoaded;

  // Used only for tests to notify that shutdown tasks are completed on
  // the DB sequence.
  base::OnceClosure shutdown_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SessionStoreImpl> weak_ptr_factory_{this};
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_STORE_IMPL_H_
