// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/sqlite_trust_token_persister.h"

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_split.h"
#include "components/sqlite_proto/key_value_data.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/proto/storage.pb.h"
#include "services/network/trust_tokens/trust_token_database_owner.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace network {

namespace {

const char kIssuerToplevelKeySeparator[] = " ";

// Converts an (issuer, top-level origin) pair keying Trust Tokens state into a
// unique string suitable for indexing persistent storage. Changes should be
// kept in sync with |FromKey|.
std::string ToKey(const SuitableTrustTokenOrigin& issuer,
                  const SuitableTrustTokenOrigin& toplevel) {
  // U+0020 space is a character forbidden in schemes/hosts/ports, so it
  // shouldn't appear in the serialization of either origin, preventing
  // collisions.
  return issuer.Serialize() + kIssuerToplevelKeySeparator +
         toplevel.Serialize();
}

// Attempts to deserialize |key_from_database| fresh off the disk. This might
// not be the output of |ToKey| because of corruption during storage on disk:
// returns false on failure.
//
// The parameters |issuer| and |toplevel| are pointers-to-optionals because
// SuitableTrustTokenOrigin does not have a default constructor.
bool FromKey(base::StringPiece key_from_database,
             base::Optional<SuitableTrustTokenOrigin>* issuer,
             base::Optional<SuitableTrustTokenOrigin>* toplevel) {
  DCHECK(issuer);
  DCHECK(toplevel);

  auto pieces =
      base::SplitStringPiece(key_from_database, kIssuerToplevelKeySeparator,
                             base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  if (pieces.size() != 2)
    return false;

  *issuer = SuitableTrustTokenOrigin::Create(GURL(pieces[0]));
  *toplevel = SuitableTrustTokenOrigin::Create(GURL(pieces[1]));
  return issuer->has_value() && toplevel->has_value();
}

void OnDatabaseOwnerCreated(
    base::OnceCallback<void(std::unique_ptr<SQLiteTrustTokenPersister>)>
        on_done_initializing,
    std::unique_ptr<TrustTokenDatabaseOwner> database_owner) {
  auto ret =
      std::make_unique<SQLiteTrustTokenPersister>(std::move(database_owner));
  std::move(on_done_initializing).Run(std::move(ret));
}

template <typename T>
bool DeleteOriginKeyedKeyValueData(
    base::RepeatingCallback<bool(const SuitableTrustTokenOrigin&)> matcher,
    sqlite_proto::KeyValueData<T>* key_value_data) {
  DCHECK(key_value_data);

  std::vector<std::string> keys_to_delete;
  bool data_from_filter_was_deleted = false;

  for (const auto& kv : key_value_data->GetAllCached()) {
    // Creation can fail if the record was corrupted on disk.
    base::Optional<SuitableTrustTokenOrigin> maybe_key =
        SuitableTrustTokenOrigin::Create(GURL(kv.first));

    // If the record's key is corrupt, delete the record no matter what, but
    // don't record the deletion request as having led to data being deleted.
    if (!maybe_key) {
      keys_to_delete.push_back(kv.first);
      continue;
    }

    if (matcher.Run(*maybe_key)) {
      keys_to_delete.push_back(kv.first);
      data_from_filter_was_deleted = true;
    }
  }

  key_value_data->DeleteData(keys_to_delete);

  return data_from_filter_was_deleted;
}

bool DeleteMatchingIssuerToplevelPairData(
    base::RepeatingCallback<bool(const SuitableTrustTokenOrigin&)> matcher,
    sqlite_proto::KeyValueData<TrustTokenIssuerToplevelPairConfig>*
        key_value_data) {
  std::vector<std::string> keys_to_delete;

  bool data_from_filter_was_deleted = false;

  for (const auto& kv : key_value_data->GetAllCached()) {
    base::Optional<SuitableTrustTokenOrigin> maybe_issuer;
    base::Optional<SuitableTrustTokenOrigin> maybe_toplevel;

    // If the record's key is corrupt, delete the record no matter what, but
    // don't record the deletion request as having led to data being deleted.
    if (!FromKey(kv.first, &maybe_issuer, &maybe_toplevel)) {
      keys_to_delete.push_back(kv.first);
      continue;
    }

    if (matcher.Run(*maybe_issuer) || matcher.Run(*maybe_toplevel)) {
      keys_to_delete.push_back(kv.first);
      data_from_filter_was_deleted = true;
    }
  }

  key_value_data->DeleteData(keys_to_delete);

  return data_from_filter_was_deleted;
}

}  // namespace

SQLiteTrustTokenPersister::SQLiteTrustTokenPersister(
    std::unique_ptr<TrustTokenDatabaseOwner> database_owner)
    : database_owner_(std::move(database_owner)) {}

SQLiteTrustTokenPersister::~SQLiteTrustTokenPersister() = default;

void SQLiteTrustTokenPersister::CreateForFilePath(
    scoped_refptr<base::SequencedTaskRunner> db_task_runner,
    const base::FilePath& path,
    base::TimeDelta flush_delay_for_writes,
    base::OnceCallback<void(std::unique_ptr<SQLiteTrustTokenPersister>)>
        on_done_initializing) {
  TrustTokenDatabaseOwner::Create(
      /*db_opener=*/base::BindOnce(
          [](const base::FilePath& path, sql::Database* db) {
            const base::FilePath directory = path.DirName();
            if (!base::PathExists(directory) &&
                !base::CreateDirectory(directory)) {
              return false;
            }
            return db->Open(path);
          },
          path),
      db_task_runner, flush_delay_for_writes,
      base::BindOnce(&OnDatabaseOwnerCreated, std::move(on_done_initializing)));
}

std::unique_ptr<TrustTokenIssuerConfig>
SQLiteTrustTokenPersister::GetIssuerConfig(
    const SuitableTrustTokenOrigin& issuer) {
  auto* data = database_owner_->IssuerData();
  CHECK(data);

  auto ret = std::make_unique<TrustTokenIssuerConfig>();
  return data->TryGetData(issuer.Serialize(), ret.get()) ? std::move(ret)
                                                         : nullptr;
}

std::unique_ptr<TrustTokenToplevelConfig>
SQLiteTrustTokenPersister::GetToplevelConfig(
    const SuitableTrustTokenOrigin& toplevel) {
  auto* data = database_owner_->ToplevelData();
  CHECK(data);

  auto ret = std::make_unique<TrustTokenToplevelConfig>();
  return data->TryGetData(toplevel.Serialize(), ret.get()) ? std::move(ret)
                                                           : nullptr;
}

std::unique_ptr<TrustTokenIssuerToplevelPairConfig>
SQLiteTrustTokenPersister::GetIssuerToplevelPairConfig(
    const SuitableTrustTokenOrigin& issuer,
    const SuitableTrustTokenOrigin& toplevel) {
  auto* data = database_owner_->IssuerToplevelPairData();
  CHECK(data);

  auto ret = std::make_unique<TrustTokenIssuerToplevelPairConfig>();
  return data->TryGetData(ToKey(issuer, toplevel), ret.get()) ? std::move(ret)
                                                              : nullptr;
}

void SQLiteTrustTokenPersister::SetIssuerConfig(
    const SuitableTrustTokenOrigin& issuer,
    std::unique_ptr<TrustTokenIssuerConfig> config) {
  sqlite_proto::KeyValueData<TrustTokenIssuerConfig>* data =
      database_owner_->IssuerData();
  CHECK(data);
  data->UpdateData(issuer.Serialize(), *config);
}

void SQLiteTrustTokenPersister::SetToplevelConfig(
    const SuitableTrustTokenOrigin& toplevel,
    std::unique_ptr<TrustTokenToplevelConfig> config) {
  sqlite_proto::KeyValueData<TrustTokenToplevelConfig>* data =
      database_owner_->ToplevelData();
  CHECK(data);
  data->UpdateData(toplevel.Serialize(), *config);
}

void SQLiteTrustTokenPersister::SetIssuerToplevelPairConfig(
    const SuitableTrustTokenOrigin& issuer,
    const SuitableTrustTokenOrigin& toplevel,
    std::unique_ptr<TrustTokenIssuerToplevelPairConfig> config) {
  sqlite_proto::KeyValueData<TrustTokenIssuerToplevelPairConfig>* data =
      database_owner_->IssuerToplevelPairData();
  CHECK(data);

  data->UpdateData(ToKey(issuer, toplevel), *config);
}

bool SQLiteTrustTokenPersister::DeleteForOrigins(
    base::RepeatingCallback<bool(const SuitableTrustTokenOrigin&)> matcher) {
  bool any_data_was_deleted = false;
  any_data_was_deleted |=
      DeleteOriginKeyedKeyValueData(matcher, database_owner_->IssuerData());

  any_data_was_deleted |=
      DeleteOriginKeyedKeyValueData(matcher, database_owner_->ToplevelData());

  any_data_was_deleted |= DeleteMatchingIssuerToplevelPairData(
      matcher, database_owner_->IssuerToplevelPairData());

  return any_data_was_deleted;
}

base::flat_map<SuitableTrustTokenOrigin, int>
SQLiteTrustTokenPersister::GetStoredTrustTokenCounts() {
  base::flat_map<SuitableTrustTokenOrigin, int> result;
  sqlite_proto::KeyValueData<TrustTokenIssuerConfig>* data =
      database_owner_->IssuerData();

  for (const auto& kv : data->GetAllCached()) {
    base::Optional<SuitableTrustTokenOrigin> origin =
        SuitableTrustTokenOrigin::Create(GURL(kv.first));
    // The Create call can fail when the SQLite data was corrupted on the disk.
    if (origin) {
      result.emplace(std::move(*origin), kv.second.tokens_size());
    }
  }

  return result;
}

}  // namespace network
