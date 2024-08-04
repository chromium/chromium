// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_SQLITE_TRUST_TOKEN_PERSISTER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_SQLITE_TRUST_TOKEN_PERSISTER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/task/task_traits.h"
#include "components/sqlite_proto/key_value_data.h"
#include "services/network/trust_tokens/proto/storage.pb.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_database_owner.h"
#include "services/network/trust_tokens/trust_token_persister.h"
#include "sql/database.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace network {

// An SQLiteTrustTokenPersister implements low-level get and put operations on
// Trust Tokens types by managing a collection of tables in an underlying SQLite
// database.
//
// It uses the //components/sqlite_proto database management
// utility to avoid dealing with too much database logic directly.
class SQLiteTrustTokenPersister : public TrustTokenPersister {
 public:
  // Constructs a SQLiteTrustTokenPersister backed by |database_owner|.
  explicit SQLiteTrustTokenPersister(
      std::unique_ptr<TrustTokenDatabaseOwner> database_owner);

  ~SQLiteTrustTokenPersister() override;

  // Constructs a SQLiteTrustTokenPersister backed by an on-disk
  // database:
  // - |db_task_runner| will be used for posting blocking database IO;
  // - |path| will store the database; if its parent directory doesn't exist,
  // the method will attempt to create the directory.
  // - |flush_delay_for_writes| is the maximum time before each write is flushed
  // to the underlying database.
  //
  // |on_done_initializing| will be called once the persister's underlying
  // state has been initialized from disk.
  //
  // If initialization fails, |on_done_initializing| will still be provided a
  // non-null pointer to a usable SQLiteTrustTokenPersister, but the persister
  // will only cache writes to memory, rather than persist them to disk.
  static void CreateForFilePath(
      scoped_refptr<base::SequencedTaskRunner> db_task_runner,
      const base::FilePath& path,
      base::TimeDelta flush_delay_for_writes,
      base::OnceCallback<void(std::unique_ptr<SQLiteTrustTokenPersister>)>
          on_done_initializing);

  // TrustTokenPersister implementation:

  // Each getter returns nullptr when the requested record was not found.
  std::unique_ptr<TrustTokenIssuerConfig> GetIssuerConfig(
      const SuitableTrustTokenOrigin& issuer) override;
  std::unique_ptr<TrustTokenToplevelConfig> GetToplevelConfig(
      const SuitableTrustTokenOrigin& toplevel) override;
  std::unique_ptr<TrustTokenIssuerToplevelPairConfig>
  GetIssuerToplevelPairConfig(
      const SuitableTrustTokenOrigin& issuer,
      const SuitableTrustTokenOrigin& toplevel) override;

  void SetIssuerConfig(const SuitableTrustTokenOrigin& issuer,
                       std::unique_ptr<TrustTokenIssuerConfig> config) override;
  void SetToplevelConfig(
      const SuitableTrustTokenOrigin& toplevel,
      std::unique_ptr<TrustTokenToplevelConfig> config) override;
  void SetIssuerToplevelPairConfig(
      const SuitableTrustTokenOrigin& issuer,
      const SuitableTrustTokenOrigin& toplevel,
      std::unique_ptr<TrustTokenIssuerToplevelPairConfig> config) override;

  bool DeleteIssuerConfig(PSTKeyMatcher key_matcher,
                          PSTTimeMatcher time_matcher) override;
  bool DeleteToplevelConfig(PSTKeyMatcher key_matcher) override;
  bool DeleteIssuerToplevelPairConfig(PSTKeyMatcher key_matcher,
                                      PSTTimeMatcher time_matcher) override;

  base::flat_map<SuitableTrustTokenOrigin, int> GetStoredTrustTokenCounts()
      override;
  IssuerRedemptionRecordMap GetRedemptionRecords() override;

 private:
  // Manages the underlying database.
  std::unique_ptr<TrustTokenDatabaseOwner> database_owner_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_SQLITE_TRUST_TOKEN_PERSISTER_H_
