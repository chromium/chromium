// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/sqlite_trust_token_persister.h"

#include <optional>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sqlite_proto/key_value_data.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/proto/storage.pb.h"
#include "services/network/trust_tokens/trust_token_database_owner.h"
#include "services/network/trust_tokens/types.h"
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
bool FromKey(std::string_view key_from_database,
             std::optional<SuitableTrustTokenOrigin>* issuer,
             std::optional<SuitableTrustTokenOrigin>* toplevel) {
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
  // Both last_redemption and penultimate_redemption should be set. Serializing
  // config will fail otherwise.
  CHECK(config->has_last_redemption());
  CHECK(config->has_penultimate_redemption());
  sqlite_proto::KeyValueData<TrustTokenIssuerToplevelPairConfig>* data =
      database_owner_->IssuerToplevelPairData();
  CHECK(data);

  data->UpdateData(ToKey(issuer, toplevel), *config);
}

bool SQLiteTrustTokenPersister::DeleteIssuerConfig(
    PSTKeyMatcher key_matcher,
    PSTTimeMatcher time_matcher) {
  sqlite_proto::KeyValueData<TrustTokenIssuerConfig>* issuer_data =
      database_owner_->IssuerData();
  DCHECK(issuer_data);

  std::vector<std::string> keys_to_delete;
  std::vector<std::pair<std::string, TrustTokenIssuerConfig>>
      key_value_pairs_to_update;
  bool data_deleted = false;

  for (const auto& kv : issuer_data->GetAllCached()) {
    // Creation can fail if the record was corrupted on disk.
    std::optional<SuitableTrustTokenOrigin> maybe_key =
        SuitableTrustTokenOrigin::Create(GURL(kv.first));

    // If the record's key is corrupt, delete the record no matter what, but
    // don't record the deletion request as having led to data being deleted.
    if (!maybe_key) {
      keys_to_delete.push_back(kv.first);
      continue;
    }

    if (!key_matcher.Run(*maybe_key)) {
      continue;
    }

    auto new_issuer_config = kv.second;
    // clear all tokens first, we will add them back if they are not a match
    new_issuer_config.clear_tokens();
    for (const auto& token : kv.second.tokens()) {
      if (token.has_creation_time()) {
        const base::Time creation_time =
            internal::TimestampToTime(token.creation_time());
        if (!time_matcher.Run(creation_time)) {
          // add token back to new issuer config
          TrustToken* new_token = new_issuer_config.add_tokens();
          *new_token = token;
        }  // else do not add token back to new_issuer_config
      }    // else token has no creation time, delete to be on the safe side
    }
    if (new_issuer_config.tokens().size() == 0) {
      // all tokens are deleted, or there were no tokens to begin with
      keys_to_delete.push_back(kv.first);
      data_deleted = true;
    } else if (new_issuer_config.tokens().size() != kv.second.tokens().size()) {
      // some tokens deleted, at least one token remains
      key_value_pairs_to_update.emplace_back(kv.first,
                                             std::move(new_issuer_config));
      data_deleted = true;
    }
    // else { no change }
  }

  issuer_data->DeleteData(keys_to_delete);
  for (const auto& kv : key_value_pairs_to_update) {
    issuer_data->UpdateData(kv.first, std::move(kv.second));
  }

  return data_deleted;
}

bool SQLiteTrustTokenPersister::DeleteToplevelConfig(
    PSTKeyMatcher key_matcher) {
  // what to pass to matcher if there is no creation time?
  sqlite_proto::KeyValueData<TrustTokenToplevelConfig>* toplevel_data =
      database_owner_->ToplevelData();
  DCHECK(toplevel_data);
  bool data_deleted = false;
  std::vector<std::string> keys_to_delete;
  for (const auto& kv : toplevel_data->GetAllCached()) {
    // Creation can fail if the record was corrupted on disk.
    std::optional<SuitableTrustTokenOrigin> maybe_key =
        SuitableTrustTokenOrigin::Create(GURL(kv.first));

    // If the record's key is corrupt, delete the record no matter what, but
    // don't record the deletion request as having led to data being deleted.
    if (!maybe_key) {
      keys_to_delete.push_back(kv.first);
      continue;
    }

    if (key_matcher.Run(*maybe_key)) {
      keys_to_delete.push_back(kv.first);
      data_deleted = true;
    }
  }
  toplevel_data->DeleteData(keys_to_delete);
  return data_deleted;
}

bool SQLiteTrustTokenPersister::DeleteIssuerToplevelPairConfig(
    PSTKeyMatcher key_matcher,
    PSTTimeMatcher time_matcher) {
  sqlite_proto::KeyValueData<TrustTokenIssuerToplevelPairConfig>* pair_data =
      database_owner_->IssuerToplevelPairData();
  DCHECK(pair_data);
  bool data_deleted = false;
  std::vector<std::string> keys_to_delete;
  for (const auto& kv : pair_data->GetAllCached()) {
    std::optional<SuitableTrustTokenOrigin> maybe_issuer;
    std::optional<SuitableTrustTokenOrigin> maybe_toplevel;
    // If the record's key is corrupt, delete the record no matter what, but
    // don't record the deletion request as having led to data being deleted.
    if (!FromKey(kv.first, &maybe_issuer, &maybe_toplevel)) {
      keys_to_delete.push_back(kv.first);
      continue;
    }

    if (!key_matcher.Run(*maybe_issuer) && !key_matcher.Run(*maybe_toplevel)) {
      continue;
    }

    const TrustTokenIssuerToplevelPairConfig& pair_config = kv.second;
    if (!pair_config.has_redemption_record()) {
      keys_to_delete.push_back(kv.first);
      data_deleted = true;
      continue;
    }

    auto redemption_record = pair_config.redemption_record();
    if (redemption_record.has_creation_time()) {
      const base::Time creation_time =
          internal::TimestampToTime(redemption_record.creation_time());
      if (time_matcher.Run(creation_time)) {
        keys_to_delete.push_back(kv.first);
        data_deleted = true;
      }
    } else {
      // no creation time for RR, delete to be on the safe side
      keys_to_delete.push_back(kv.first);
      data_deleted = true;
    }
  }
  pair_data->DeleteData(keys_to_delete);
  return data_deleted;
}

base::flat_map<SuitableTrustTokenOrigin, int>
SQLiteTrustTokenPersister::GetStoredTrustTokenCounts() {
  base::flat_map<SuitableTrustTokenOrigin, int> result;
  sqlite_proto::KeyValueData<TrustTokenIssuerConfig>* data =
      database_owner_->IssuerData();

  for (const auto& kv : data->GetAllCached()) {
    std::optional<SuitableTrustTokenOrigin> origin =
        SuitableTrustTokenOrigin::Create(GURL(kv.first));
    // The Create call can fail when the SQLite data was corrupted on the disk.
    if (origin) {
      result.emplace(std::move(*origin), kv.second.tokens_size());
    }
  }

  return result;
}

IssuerRedemptionRecordMap SQLiteTrustTokenPersister::GetRedemptionRecords() {
  IssuerRedemptionRecordMap result;

  sqlite_proto::KeyValueData<TrustTokenIssuerToplevelPairConfig>* pair_data =
      database_owner_->IssuerToplevelPairData();

  for (const auto& kv : pair_data->GetAllCached()) {
    std::optional<SuitableTrustTokenOrigin> maybe_issuer;
    std::optional<SuitableTrustTokenOrigin> maybe_toplevel;
    if (!FromKey(kv.first, &maybe_issuer, &maybe_toplevel)) {
      continue;
    }
    CHECK(maybe_issuer);
    CHECK(maybe_toplevel);

    const TrustTokenIssuerToplevelPairConfig& pair_config = kv.second;
    if (!pair_config.has_redemption_record()) {
      continue;
    }

    const base::Time last_redemption =
        internal::TimestampToTime(pair_config.last_redemption());

    auto entry = mojom::ToplevelRedemptionRecord::New(
        std::move(maybe_toplevel.value()), std::move(last_redemption));

    if (auto it = result.find(maybe_issuer->origin()); it != result.end()) {
      it->second.push_back(std::move(entry));
      continue;
    }
    std::vector<mojom::ToplevelRedemptionRecordPtr> v = {};
    v.push_back(std::move(entry));
    result.emplace(std::move(maybe_issuer->origin()), std::move(v));
  }
  return result;
}

}  // namespace network
