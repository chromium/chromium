// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_DATABASE_OWNER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_DATABASE_OWNER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/sqlite_proto/key_value_data.h"
#include "components/sqlite_proto/key_value_table.h"
#include "components/sqlite_proto/proto_table_manager.h"
#include "services/network/trust_tokens/proto/storage.pb.h"

namespace sql {

class Database;

}  // namespace sql

namespace network {

// A TrustTokenDatabaseOwner does two things:
// 1. It constructs and initializes an SQLite database, delegating some of this
// work to the //components/sqlite_proto library.
// 2. It provides, via the sqlite_proto::KeyValueData interface, access to a
// number of tables in the database that it owns.
class TrustTokenDatabaseOwner final {
 public:
  // Constructs and asynchronously initializes a new TrustTokenDatabaseOwner,
  // calling |on_done_initializing| with an owning pointer to the constructed
  // object once initialization has finished and the object is ready to use.
  //
  // Posts a task to |db_task_runner| to initialize all pertinent DB state on
  // the DB sequence.
  //
  // |db_opener| is a callback that opens the given sql::Database*.  This allows
  // opening in memory for testing (for instance). In normal usage, this will
  // probably open the given database on disk at a prespecified filepath.
  //
  // |flush_delay_for_writes| is the maximum time before each write is flushed
  // to the underlying database.
  static void Create(
      base::OnceCallback<bool(sql::Database*)> db_opener,
      scoped_refptr<base::SequencedTaskRunner> db_task_runner,
      base::TimeDelta flush_delay_for_writes,
      base::OnceCallback<void(std::unique_ptr<TrustTokenDatabaseOwner>)>
          on_done_initializing);

  // Except when using the same sequence for construction and database
  // operations, the destructor uses the DB task runner to destroy the
  // backing database asynchronously.
  ~TrustTokenDatabaseOwner();

  TrustTokenDatabaseOwner(const TrustTokenDatabaseOwner&) = delete;
  TrustTokenDatabaseOwner& operator=(const TrustTokenDatabaseOwner&) = delete;

  // Use these getters to execute operations (get, put, delete) on the
  // underlying data.
  sqlite_proto::KeyValueData<TrustTokenIssuerConfig>* IssuerData();
  sqlite_proto::KeyValueData<TrustTokenToplevelConfig>* ToplevelData();
  sqlite_proto::KeyValueData<TrustTokenIssuerToplevelPairConfig>*
  IssuerToplevelPairData();

 private:
  TrustTokenDatabaseOwner(
      base::OnceCallback<bool(sql::Database*)> db_opener,
      scoped_refptr<base::SequencedTaskRunner> db_task_runner,
      base::TimeDelta flush_delay_for_writes,
      base::OnceCallback<void(std::unique_ptr<TrustTokenDatabaseOwner>)>
          on_done_initializing);

  // Opens the backing database by passing it to |db_opener|, then calls into
  // ProtoTableManager and KeyValueData's on-database-sequence initialization
  // methods (the former in order to create the tables and execute a schema
  // upgrade if necessary, the latter in order to read data into memory).
  void InitializeMembersOnDbSequence(
      base::OnceCallback<bool(sql::Database*)> db_opener);

  // Wraps |this| in a unique_ptr and passes it to |on_done_initializing_|.
  void FinishInitializationOnMainSequence();

  base::OnceCallback<void(std::unique_ptr<TrustTokenDatabaseOwner>)>
      on_done_initializing_;

  // |*table_manager_| is responsible for constructing the database's tables and
  // scheduling database tasks.
  scoped_refptr<sqlite_proto::ProtoTableManager> table_manager_;

  // Keep a handle on the DB task runner so that the destructor
  // can use the DB sequence to clean up the DB.
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;

  // The backing database.
  std::unique_ptr<sql::Database> backing_database_;

  // Each KeyValueData/KeyValueTable pair is responsible for executing SQL
  // operations against a particular database table. The KeyValueTables help
  // with serializing/deserializing proto objects, and the KeyValueData objects
  // batch writes and cache reads.
  std::unique_ptr<sqlite_proto::KeyValueTable<TrustTokenIssuerConfig>>
      issuer_table_;
  std::unique_ptr<sqlite_proto::KeyValueData<TrustTokenIssuerConfig>>
      issuer_data_;

  std::unique_ptr<sqlite_proto::KeyValueTable<TrustTokenToplevelConfig>>
      toplevel_table_;
  std::unique_ptr<sqlite_proto::KeyValueData<TrustTokenToplevelConfig>>
      toplevel_data_;

  std::unique_ptr<
      sqlite_proto::KeyValueTable<TrustTokenIssuerToplevelPairConfig>>
      issuer_toplevel_pair_table_;
  std::unique_ptr<
      sqlite_proto::KeyValueData<TrustTokenIssuerToplevelPairConfig>>
      issuer_toplevel_pair_data_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_DATABASE_OWNER_H_
