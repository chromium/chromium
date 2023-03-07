// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_storage.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_k_anonymity_manager.h"
#include "content/browser/interest_group/interest_group_update.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/sqlite/sqlite3.h"
#include "url/origin.h"

namespace content {

namespace {

using auction_worklet::mojom::BiddingBrowserSignalsPtr;
using auction_worklet::mojom::PreviousWinPtr;
using SellerCapabilitiesType = blink::SellerCapabilitiesType;

const base::FilePath::CharType kDatabasePath[] =
    FILE_PATH_LITERAL("InterestGroups");

// Version number of the database.
//
// Version 1 - 2021/03 - crrev.com/c/2757425
// Version 2 - 2021/08 - crrev.com/c/3097715
// Version 3 - 2021/09 - crrev.com/c/3165576
// Version 4 - 2021/10 - crrev.com/c/3172863
// Version 5 - 2021/10 - crrev.com/c/3067804
// Version 6 - 2021/12 - crrev.com/c/3330516
// Version 7 - 2022/03 - crrev.com/c/3517534
// Version 8 - 2022/06 - crrev.com/c/3696265
// Version 9 - 2022/07 - crrev.com/c/3780305
// Version 10 - 2022/08 - crrev.com/c/3818142
// Version 13 - 2023/01 - crrev.com/c/4167800
//
// Version 1 adds a table for interest groups.
// Version 2 adds a column for rate limiting interest group updates.
// Version 3 adds a field for ad components.
// Version 4 adds joining origin and url.
// Version 5 adds k-anonymity tables and fields.
// Version 6 adds WebAssembly helper url.
// Version 7 changes an index, adds interest group priority.
// Version 8 adds the execution_mode field to interest groups.
// Version 9 changes bid_history and join_history to daily counts.
// Version 10 changes k-anonymity table so it doesn't split by type.
// Version 11 adds priority vector support and time a group was joined.
// Version 12 adds seller capabilities fields.
// Version 13 adds ad size-related fields (ad_sizes & size_groups).
const int kCurrentVersionNumber = 13;

// Earliest version of the code which can use a |kCurrentVersionNumber|
// database without failing.
const int kCompatibleVersionNumber = 13;

// Latest version of the database that cannot be upgraded to
// |kCurrentVersionNumber| without razing the database.
const int kDeprecatedVersionNumber = 5;

std::string Serialize(base::ValueView value_view) {
  std::string json_output;
  JSONStringValueSerializer serializer(&json_output);
  serializer.Serialize(value_view);
  return json_output;
}
std::unique_ptr<base::Value> DeserializeValue(
    const std::string& serialized_value) {
  if (serialized_value.empty())
    return {};
  JSONStringValueDeserializer deserializer(serialized_value);
  return deserializer.Deserialize(/*error_code=*/nullptr,
                                  /*error_message=*/nullptr);
}

std::string Serialize(const url::Origin& origin) {
  return origin.Serialize();
}
url::Origin DeserializeOrigin(const std::string& serialized_origin) {
  return url::Origin::Create(GURL(serialized_origin));
}

std::string Serialize(const absl::optional<GURL>& url) {
  if (!url)
    return std::string();
  return url->spec();
}
absl::optional<GURL> DeserializeURL(const std::string& serialized_url) {
  GURL result(serialized_url);
  if (result.is_empty())
    return absl::nullopt;
  return result;
}

base::Value ToValue(const blink::InterestGroup::Ad& ad) {
  base::Value value(base::Value::Type::DICT);
  base::Value::Dict& dict = value.GetDict();
  dict.Set("url", ad.render_url.spec());
  if (ad.metadata)
    dict.Set("metadata", ad.metadata.value());
  return value;
}
blink::InterestGroup::Ad FromInterestGroupAdValue(
    const base::Value::Dict& dict) {
  blink::InterestGroup::Ad result;
  const std::string* maybe_url = dict.FindString("url");
  if (maybe_url)
    result.render_url = GURL(*maybe_url);
  const std::string* maybe_metadata = dict.FindString("metadata");
  if (maybe_metadata)
    result.metadata = *maybe_metadata;
  return result;
}

std::string Serialize(
    const absl::optional<base::flat_map<std::string, double>>& flat_map) {
  if (!flat_map)
    return std::string();
  base::Value::Dict dict;
  for (const auto& key_value_pair : *flat_map) {
    dict.Set(key_value_pair.first, key_value_pair.second);
  }
  return Serialize(base::Value(std::move(dict)));
}
absl::optional<base::flat_map<std::string, double>> DeserializeStringDoubleMap(
    const std::string& serialized_flat_map) {
  std::unique_ptr<base::Value> flat_map_value =
      DeserializeValue(serialized_flat_map);
  if (!flat_map_value || !flat_map_value->is_dict())
    return absl::nullopt;

  // Extract all key/values pairs to a vector before writing to a flat_map,
  // since flat_map insertion is O(n).
  std::vector<std::pair<std::string, double>> pairs;
  for (const auto pair : flat_map_value->GetDict()) {
    if (!pair.second.is_double())
      return absl::nullopt;
    pairs.emplace_back(pair.first, pair.second.GetDouble());
  }
  return base::flat_map<std::string, double>(std::move(pairs));
}

std::string Serialize(
    const absl::optional<std::vector<blink::InterestGroup::Ad>>& ads) {
  if (!ads)
    return std::string();
  base::Value::List list;
  for (const auto& ad : ads.value()) {
    list.Append(ToValue(ad));
  }
  return Serialize(list);
}
absl::optional<std::vector<blink::InterestGroup::Ad>>
DeserializeInterestGroupAdVector(const std::string& serialized_ads) {
  std::unique_ptr<base::Value> ads_value = DeserializeValue(serialized_ads);
  if (!ads_value || !ads_value->is_list())
    return absl::nullopt;
  std::vector<blink::InterestGroup::Ad> result;
  for (const auto& ad_value : ads_value->GetList()) {
    const base::Value::Dict* dict = ad_value.GetIfDict();
    if (dict)
      result.emplace_back(FromInterestGroupAdValue(*dict));
  }
  return result;
}

std::string Serialize(
    const absl::optional<
        base::flat_map<std::string, blink::InterestGroup::Size>>& ad_sizes) {
  if (!ad_sizes) {
    return std::string();
  }
  base::Value::Dict dict;
  for (const auto& key_value_pair : *ad_sizes) {
    base::Value::Dict size_dict;
    size_dict.Set("width", key_value_pair.second.width);
    size_dict.Set("width_units",
                  static_cast<int>(key_value_pair.second.width_units));
    size_dict.Set("height", key_value_pair.second.height);
    size_dict.Set("height_units",
                  static_cast<int>(key_value_pair.second.height_units));
    dict.Set(key_value_pair.first,
             Serialize(base::Value(std::move(size_dict))));
  }
  return Serialize(base::Value(std::move(dict)));
}
absl::optional<base::flat_map<std::string, blink::InterestGroup::Size>>
DeserializeStringSizeMap(const std::string& serialized_sizes) {
  std::unique_ptr<base::Value> dict = DeserializeValue(serialized_sizes);
  if (!dict || !dict->is_dict()) {
    return absl::nullopt;
  }
  std::vector<std::pair<std::string, blink::InterestGroup::Size>> result;
  for (std::pair<const std::string&, base::Value&> entry : dict->GetDict()) {
    std::unique_ptr<base::Value> ads_size =
        DeserializeValue(entry.second.GetString());
    const base::Value::Dict* size_dict = ads_size->GetIfDict();
    DCHECK(size_dict);
    const base::Value* width_val = size_dict->Find("width");
    const base::Value* width_units_val = size_dict->Find("width_units");
    const base::Value* height_val = size_dict->Find("height");
    const base::Value* height_units_val = size_dict->Find("width_units");
    if (!width_val || !width_units_val || !height_val || !height_units_val) {
      return absl::nullopt;
    }
    result.emplace_back(entry.first,
                        blink::InterestGroup::Size(
                            width_val->GetDouble(),
                            static_cast<blink::InterestGroup::Size::LengthUnit>(
                                width_units_val->GetInt()),
                            height_val->GetDouble(),
                            static_cast<blink::InterestGroup::Size::LengthUnit>(
                                height_units_val->GetInt())));
  }
  return result;
}

std::string Serialize(
    const absl::optional<base::flat_map<std::string, std::vector<std::string>>>&
        size_groups) {
  if (!size_groups) {
    return std::string();
  }
  base::Value::Dict dict;
  for (const auto& key_value_pair : *size_groups) {
    base::Value::List list;
    for (const auto& s : key_value_pair.second) {
      list.Append(s);
    }
    dict.Set(key_value_pair.first, Serialize(list));
  }
  return Serialize(base::Value(std::move(dict)));
}
absl::optional<base::flat_map<std::string, std::vector<std::string>>>
DeserializeStringStringVectorMap(const std::string& serialized_groups) {
  std::unique_ptr<base::Value> dict = DeserializeValue(serialized_groups);
  if (!dict || !dict->is_dict()) {
    return absl::nullopt;
  }
  std::vector<std::pair<std::string, std::vector<std::string>>> result;
  for (std::pair<const std::string&, base::Value&> entry : dict->GetDict()) {
    std::unique_ptr<base::Value> list =
        DeserializeValue(entry.second.GetString());
    DCHECK(list && list->is_list());
    std::vector<std::string> result_sizes;
    for (base::Value& size : list->GetList()) {
      result_sizes.emplace_back(size.GetString());
    }
    result.emplace_back(entry.first, result_sizes);
  }
  return result;
}

std::string Serialize(const absl::optional<std::vector<std::string>>& strings) {
  if (!strings)
    return std::string();
  base::Value::List list;
  for (const auto& s : strings.value())
    list.Append(s);
  return Serialize(list);
}

absl::optional<std::vector<std::string>> DeserializeStringVector(
    const std::string& serialized_vector) {
  std::unique_ptr<base::Value> list = DeserializeValue(serialized_vector);
  if (!list || !list->is_list())
    return absl::nullopt;
  std::vector<std::string> result;
  for (const auto& value : list->GetList())
    result.push_back(value.GetString());
  return result;
}

int64_t Serialize(SellerCapabilitiesType capabilities) {
  uint64_t result = capabilities.ToEnumBitmask();
  // Supporting 64 or more seller capabilities will require a different
  // serialization format.
  DCHECK(result <= std::numeric_limits<int64_t>::max());
  return static_cast<int64_t>(result);
}
SellerCapabilitiesType DeserializeSellerCapabilities(int64_t serialized) {
  DCHECK(serialized >= 0);
  return SellerCapabilitiesType::FromEnumBitmask(serialized);
}

std::string Serialize(
    const absl::optional<base::flat_map<url::Origin, SellerCapabilitiesType>>&
        flat_map) {
  if (!flat_map)
    return std::string();
  base::Value::Dict dict;
  for (const auto& key_value_pair : *flat_map) {
    dict.Set(Serialize(key_value_pair.first),
             base::NumberToString(Serialize(key_value_pair.second)));
  }
  return Serialize(base::Value(std::move(dict)));
}
absl::optional<base::flat_map<url::Origin, SellerCapabilitiesType>>
DeserializeSellerCapabilitiesMap(const std::string& serialized) {
  std::unique_ptr<base::Value> dict = DeserializeValue(serialized);
  if (!dict || !dict->is_dict())
    return absl::nullopt;
  std::vector<std::pair<url::Origin, SellerCapabilitiesType>> result;
  for (std::pair<const std::string&, base::Value&> entry : dict->GetDict()) {
    std::string* value_string = entry.second.GetIfString();
    if (!value_string)
      return absl::nullopt;
    int64_t value_bitmask;
    if (!base::StringToInt64(*value_string, &value_bitmask))
      return absl::nullopt;
    result.emplace_back(DeserializeOrigin(entry.first),
                        DeserializeSellerCapabilities(value_bitmask));
  }
  return result;
}

StorageInterestGroup::KAnonymityData DefaultKAnonymityData(
    const std::string& key) {
  return {key, /*is_k_anonymous=*/false, /*last_updated=*/base::Time::Min()};
}

// Merges new `priority_signals_overrides` received from an update with an
// existing set of overrides store with an interest group. Populates `overrides`
// if it was previously null.
void MergePrioritySignalsOverrides(
    const base::flat_map<std::string, absl::optional<double>>& update_data,
    absl::optional<base::flat_map<std::string, double>>&
        priority_signals_overrides) {
  if (!priority_signals_overrides)
    priority_signals_overrides.emplace();
  for (const auto& pair : update_data) {
    if (!pair.second.has_value()) {
      priority_signals_overrides->erase(pair.first);
      continue;
    }
    priority_signals_overrides->insert_or_assign(pair.first, *pair.second);
  }
}

// Same as above, but takes a map with PrioritySignalsDoublePtrs instead of
// absl::optional<double>s. This isn't much more code than it takes to convert
// the flat_map of PrioritySignalsDoublePtr to one of optionals, so just
// duplicate the logic.
void MergePrioritySignalsOverrides(
    const base::flat_map<std::string,
                         auction_worklet::mojom::PrioritySignalsDoublePtr>&
        update_data,
    absl::optional<base::flat_map<std::string, double>>&
        priority_signals_overrides) {
  if (!priority_signals_overrides)
    priority_signals_overrides.emplace();
  for (const auto& pair : update_data) {
    if (!pair.second) {
      priority_signals_overrides->erase(pair.first);
      continue;
    }
    priority_signals_overrides->insert_or_assign(pair.first,
                                                 pair.second->value);
  }
}

// Adds indices to the `interest_group` table. Called after the table has been
// created.
bool CreateInterestGroupIndices(sql::Database& db) {
  // Index on group expiration. Owner and Name are only here to speed up
  // queries that don't need the full group.
  DCHECK(!db.DoesIndexExist("interest_group_expiration"));
  static const char kInterestGroupExpirationIndexSql[] =
      // clang-format off
      "CREATE INDEX interest_group_expiration"
      " ON interest_groups(expiration DESC, owner, name)";
  // clang-format on
  if (!db.Execute(kInterestGroupExpirationIndexSql))
    return false;

  // Index on group expiration by owner.
  DCHECK(!db.DoesIndexExist("interest_group_owner"));
  static const char kInterestGroupOwnerIndexSql[] =
      // clang-format off
      "CREATE INDEX interest_group_owner"
      " ON interest_groups(owner,expiration DESC,next_update_after ASC,name)";
  // clang-format on
  if (!db.Execute(kInterestGroupOwnerIndexSql))
    return false;

  // Index on group expiration by joining origin. Owner and Name are only here
  // to speed up queries that don't need the full group.
  DCHECK(!db.DoesIndexExist("interest_group_joining_origin"));
  static const char kInterestGroupJoiningOriginIndexSql[] =
      // clang-format off
      "CREATE INDEX interest_group_joining_origin"
      " ON interest_groups(joining_origin, expiration DESC, owner, name)";
  // clang-format on
  if (!db.Execute(kInterestGroupJoiningOriginIndexSql))
    return false;

  return true;
}

// Initializes the tables, returning true on success.
// The tables cannot exist when calling this function.
bool CreateV13Schema(sql::Database& db) {
  DCHECK(!db.DoesTableExist("interest_groups"));
  static const char kInterestGroupTableSql[] =
      // clang-format off
      "CREATE TABLE interest_groups("
        "expiration INTEGER NOT NULL,"
        "last_updated INTEGER NOT NULL,"
        "next_update_after INTEGER NOT NULL,"
        "owner TEXT NOT NULL,"
        "joining_origin TEXT NOT NULL,"
        // Most recent time the interest group was joined. Called
        // `exact_join_time` to differentiate it from `join_time` in the join
        // history table, which is a day, and isn't looked up on InterestGroup
        // load.
        "exact_join_time INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "priority DOUBLE NOT NULL,"
        "enable_bidding_signals_prioritization INTEGER NOT NULL,"
        "priority_vector TEXT NOT NULL,"
        "priority_signals_overrides TEXT NOT NULL,"
        "seller_capabilities TEXT NOT NULL,"
        "all_sellers_capabilities INTEGER NOT NULL,"
        "execution_mode INTEGER NOT NULL,"
        "joining_url TEXT NOT NULL,"
        "bidding_url TEXT NOT NULL,"
        "bidding_wasm_helper_url TEXT NOT NULL,"
        "update_url TEXT NOT NULL,"
        "trusted_bidding_signals_url TEXT NOT NULL,"
        "trusted_bidding_signals_keys TEXT NOT NULL,"
        "user_bidding_signals TEXT,"
        "ads TEXT NOT NULL,"
        "ad_components TEXT NOT NULL,"
        "ad_sizes TEXT NOT NULL,"
        "size_groups TEXT NOT NULL,"
      "PRIMARY KEY(owner,name))";
  // clang-format on
  if (!db.Execute(kInterestGroupTableSql))
    return false;

  if (!CreateInterestGroupIndices(db))
    return false;

  DCHECK(!db.DoesTableExist("k_anon"));
  static const char kInterestGroupKAnonTableSql[] =
      // clang-format off
      "CREATE TABLE k_anon("
        "last_referenced_time INTEGER NOT NULL,"
        "key TEXT NOT NULL,"
        "is_k_anon INTEGER NOT NULL,"
        "last_k_anon_updated_time INTEGER NOT NULL,"
        "last_reported_to_anon_server_time INTEGER NOT NULL,"
        "PRIMARY KEY(key))";
  // clang-format on
  if (!db.Execute(kInterestGroupKAnonTableSql))
    return false;

  // Index on kanon last_referenced_time.
  DCHECK(!db.DoesIndexExist("k_anon_last_referenced_time"));
  static const char kInterestGroupKAnonLastRefIndexSql[] =
      // clang-format off
      "CREATE INDEX k_anon_last_referenced_time"
      " ON k_anon(last_referenced_time DESC)";
  // clang-format on
  if (!db.Execute(kInterestGroupKAnonLastRefIndexSql))
    return false;

  DCHECK(!db.DoesTableExist("join_history"));
  static const char kJoinHistoryTableSql[] =
      // clang-format off
      "CREATE TABLE join_history("
        "owner TEXT NOT NULL,"
        "name TEXT NOT NULL,"
        "join_time INTEGER NOT NULL,"
        "count INTEGER NOT NULL,"
      "PRIMARY KEY(owner, name, join_time) "
      "FOREIGN KEY(owner,name) REFERENCES interest_groups)";
  // clang-format on
  if (!db.Execute(kJoinHistoryTableSql))
    return false;

  DCHECK(!db.DoesTableExist("bid_history"));
  static const char kBidHistoryTableSql[] =
      // clang-format off
      "CREATE TABLE bid_history("
        "owner TEXT NOT NULL,"
        "name TEXT NOT NULL,"
        "bid_time INTEGER NOT NULL,"
        "count INTEGER NOT NULL,"
      "PRIMARY KEY(owner, name, bid_time) "
      "FOREIGN KEY(owner,name) REFERENCES interest_groups)";
  // clang-format on
  if (!db.Execute(kBidHistoryTableSql))
    return false;

  // We can't use the interest group and win time as primary keys since
  // auctions on separate pages may occur at the same time.
  DCHECK(!db.DoesTableExist("win_history"));
  static const char kWinHistoryTableSQL[] =
      // clang-format off
      "CREATE TABLE win_history("
        "owner TEXT NOT NULL,"
        "name TEXT NOT NULL,"
        "win_time INTEGER NOT NULL,"
        "ad TEXT NOT NULL,"
      "FOREIGN KEY(owner,name) REFERENCES interest_groups)";
  // clang-format on
  if (!db.Execute(kWinHistoryTableSQL))
    return false;

  DCHECK(!db.DoesIndexExist("win_history_index"));
  static const char kWinHistoryIndexSQL[] =
      // clang-format off
      "CREATE INDEX win_history_index "
      "ON win_history(owner,name,win_time DESC)";
  // clang-format on
  if (!db.Execute(kWinHistoryIndexSQL))
    return false;

  return true;
}

bool UpgradeV12SchemaToV13(sql::Database& db, sql::MetaTable& meta_table) {
  static const char kInterestGroupTableSql[] =
      // clang-format off
      "CREATE TABLE new_interest_groups("
        "expiration INTEGER NOT NULL,"
        "last_updated INTEGER NOT NULL,"
        "next_update_after INTEGER NOT NULL,"
        "owner TEXT NOT NULL,"
        "joining_origin TEXT NOT NULL,"
        "exact_join_time INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "priority DOUBLE NOT NULL,"
        "enable_bidding_signals_prioritization INTEGER NOT NULL,"
        "priority_vector TEXT NOT NULL,"
        "priority_signals_overrides TEXT NOT NULL,"
        "seller_capabilities TEXT NOT NULL,"
        "all_sellers_capabilities INTEGER NOT NULL,"
        "execution_mode INTEGER NOT NULL,"
        "joining_url TEXT NOT NULL,"
        "bidding_url TEXT NOT NULL,"
        "bidding_wasm_helper_url TEXT NOT NULL,"
        "update_url TEXT NOT NULL,"
        "trusted_bidding_signals_url TEXT NOT NULL,"
        "trusted_bidding_signals_keys TEXT NOT NULL,"
        "user_bidding_signals TEXT,"
        "ads TEXT NOT NULL,"
        "ad_components TEXT NOT NULL,"
        "ad_sizes TEXT NOT NULL,"
        "size_groups TEXT NOT NULL,"
      "PRIMARY KEY(owner,name))";
  // clang-format on
  if (!db.Execute(kInterestGroupTableSql)) {
    return false;
  }

  static const char kCopyInterestGroupTableSql[] =
      // clang-format off
      "INSERT INTO new_interest_groups "
      "SELECT expiration,"
             "last_updated,"
             "next_update_after,"
             "owner,"
             "joining_origin,"
             "exact_join_time,"
             "name,"
             "priority,"
             "enable_bidding_signals_prioritization,"
             "priority_vector,"
             "priority_signals_overrides,"
             "seller_capabilities,"
             "all_sellers_capabilities,"
             "execution_mode,"
             "joining_url,"
             "bidding_url,"
             "bidding_wasm_helper_url,"
             "update_url,"
             "trusted_bidding_signals_url,"
             "trusted_bidding_signals_keys,"
             "user_bidding_signals,"
             "ads,"
             "ad_components,"
             "''," // ad_sizes
             "''" // size_groups
      "FROM interest_groups";
  // clang-format on
  if (!db.Execute(kCopyInterestGroupTableSql)) {
    return false;
  }

  static const char kDropInterestGroupTableSql[] = "DROP TABLE interest_groups";
  if (!db.Execute(kDropInterestGroupTableSql)) {
    return false;
  }

  static const char kRenameInterestGroupTableSql[] =
      // clang-format off
      "ALTER TABLE new_interest_groups "
      "RENAME TO interest_groups";
  // clang-format on
  if (!db.Execute(kRenameInterestGroupTableSql)) {
    return false;
  }

  return CreateInterestGroupIndices(db);
}

bool UpgradeV11SchemaToV12(sql::Database& db, sql::MetaTable& meta_table) {
  static const char kInterestGroupTableSql[] =
      // clang-format off
      "CREATE TABLE new_interest_groups("
        "expiration INTEGER NOT NULL,"
        "last_updated INTEGER NOT NULL,"
        "next_update_after INTEGER NOT NULL,"
        "owner TEXT NOT NULL,"
        "joining_origin TEXT NOT NULL,"
        "exact_join_time INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "priority DOUBLE NOT NULL,"
        "enable_bidding_signals_prioritization INTEGER NOT NULL,"
        "priority_vector TEXT NOT NULL,"
        "priority_signals_overrides TEXT NOT NULL,"
        "seller_capabilities TEXT NOT NULL,"
        "all_sellers_capabilities INTEGER NOT NULL,"
        "execution_mode INTEGER NOT NULL,"
        "joining_url TEXT NOT NULL,"
        "bidding_url TEXT NOT NULL,"
        "bidding_wasm_helper_url TEXT NOT NULL,"
        "update_url TEXT NOT NULL,"
        "trusted_bidding_signals_url TEXT NOT NULL,"
        "trusted_bidding_signals_keys TEXT NOT NULL,"
        "user_bidding_signals TEXT,"
        "ads TEXT NOT NULL,"
        "ad_components TEXT NOT NULL,"
      "PRIMARY KEY(owner,name))";
  // clang-format on
  if (!db.Execute(kInterestGroupTableSql))
    return false;

  static const char kCopyInterestGroupTableSql[] =
      // clang-format off
      "INSERT INTO new_interest_groups "
      "SELECT expiration,"
             "last_updated,"
             "next_update_after,"
             "owner,"
             "joining_origin,"
             "exact_join_time,"
             "name,"
             "priority,"
             "enable_bidding_signals_prioritization,"
             "priority_vector,"
             "priority_signals_overrides,"
             "'',"  // seller_capabilities
             "0,"  // all_sellers_capabilities
             "execution_mode,"
             "joining_url,"
             "bidding_url,"
             "bidding_wasm_helper_url,"
             "update_url,"
             "trusted_bidding_signals_url,"
             "trusted_bidding_signals_keys,"
             "user_bidding_signals,"
             "ads,"
             "ad_components "
      "FROM interest_groups";
  // clang-format on
  if (!db.Execute(kCopyInterestGroupTableSql))
    return false;

  static const char kDropInterestGroupTableSql[] = "DROP TABLE interest_groups";
  if (!db.Execute(kDropInterestGroupTableSql))
    return false;

  static const char kRenameInterestGroupTableSql[] =
      // clang-format off
      "ALTER TABLE new_interest_groups "
      "RENAME TO interest_groups";
  // clang-format on
  if (!db.Execute(kRenameInterestGroupTableSql))
    return false;

  return CreateInterestGroupIndices(db);
}

bool UpgradeV10SchemaToV11(sql::Database& db, sql::MetaTable& meta_table) {
  static const char kInterestGroupTableSql[] =
      // clang-format off
      "CREATE TABLE new_interest_groups("
        "expiration INTEGER NOT NULL,"
        "last_updated INTEGER NOT NULL,"
        "next_update_after INTEGER NOT NULL,"
        "owner TEXT NOT NULL,"
        "joining_origin TEXT NOT NULL,"
        "exact_join_time INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "priority DOUBLE NOT NULL,"
        "enable_bidding_signals_prioritization INTEGER NOT NULL,"
        "priority_vector TEXT NOT NULL,"
        "priority_signals_overrides TEXT NOT NULL,"
        "execution_mode INTEGER NOT NULL,"
        "joining_url TEXT NOT NULL,"
        "bidding_url TEXT NOT NULL,"
        "bidding_wasm_helper_url TEXT NOT NULL,"
        "update_url TEXT NOT NULL,"
        "trusted_bidding_signals_url TEXT NOT NULL,"
        "trusted_bidding_signals_keys TEXT NOT NULL,"
        "user_bidding_signals TEXT,"
        "ads TEXT NOT NULL,"
        "ad_components TEXT NOT NULL,"
      "PRIMARY KEY(owner,name))";
  // clang-format on
  if (!db.Execute(kInterestGroupTableSql))
    return false;

  static const char kCopyInterestGroupTableSql[] =
      // clang-format off
      "INSERT INTO new_interest_groups "
      "SELECT expiration,"
             "last_updated,"
             "next_update_after,"
             "owner,"
             "joining_origin,"
             "last_updated," // exact_join_time
             "name,"
             "priority,"
             "0," // enable_bidding_signals_prioritization
             "''," // priority_vector
             "''," // priority_signals_overrides
             "execution_mode,"
             "joining_url,"
             "bidding_url,"
             "bidding_wasm_helper_url,"
             "update_url,"
             "trusted_bidding_signals_url,"
             "trusted_bidding_signals_keys,"
             "user_bidding_signals,"
             "ads,"
             "ad_components "
      "FROM interest_groups";
  // clang-format on
  if (!db.Execute(kCopyInterestGroupTableSql))
    return false;

  static const char kDropInterestGroupTableSql[] = "DROP TABLE interest_groups";
  if (!db.Execute(kDropInterestGroupTableSql))
    return false;

  static const char kRenameInterestGroupTableSql[] =
      // clang-format off
      "ALTER TABLE new_interest_groups "
      "RENAME TO interest_groups";
  // clang-format on
  if (!db.Execute(kRenameInterestGroupTableSql))
    return false;

  return CreateInterestGroupIndices(db);
}

bool UpgradeV9SchemaToV10(sql::Database& db, sql::MetaTable& meta_table) {
  static const char kInterestGroupKAnonTableSql[] =
      // clang-format off
      "CREATE TABLE k_anon("
        "last_referenced_time INTEGER NOT NULL,"
        "key TEXT NOT NULL,"
        "is_k_anon INTEGER NOT NULL,"
        "last_k_anon_updated_time INTEGER NOT NULL,"
        "last_reported_to_anon_server_time INTEGER NOT NULL,"
        "PRIMARY KEY(key))";
  // clang-format on
  if (!db.Execute(kInterestGroupKAnonTableSql))
    return false;

  static const char kInterestGroupKAnonLastRefIndexSql[] =
      // clang-format off
      "CREATE INDEX k_anon_last_referenced_time"
      " ON k_anon(last_referenced_time DESC)";
  // clang-format on
  if (!db.Execute(kInterestGroupKAnonLastRefIndexSql))
    return false;

  static const char kCopyKAnonTableSql[] =
      // clang-format off
      "INSERT INTO k_anon "
        "SELECT last_referenced_time,"
        "key,"
        "k_anon_count > 50 AS is_k_anon,"
        "last_k_anon_updated_time,"
        "last_reported_to_anon_server_time "
      "FROM kanon";
  // clang-format on
  if (!db.Execute(kCopyKAnonTableSql))
    return false;

  static const char kDropKanonTableSql[] = "DROP TABLE kanon";
  if (!db.Execute(kDropKanonTableSql))
    return false;

  return true;
}

bool UpgradeV8SchemaToV9(sql::Database& db, sql::MetaTable& meta_table) {
  static const char kJoinHistoryTableSql[] =
      // clang-format off
      "CREATE TABLE join_history2("
        "owner TEXT NOT NULL,"
        "name TEXT NOT NULL,"
        "join_time INTEGER NOT NULL,"
        "count INTEGER NOT NULL,"
      "PRIMARY KEY(owner, name, join_time) "
      "FOREIGN KEY(owner,name) REFERENCES interest_groups)";
  // clang-format on
  if (!db.Execute(kJoinHistoryTableSql))
    return false;

  // Consolidate old join records into a single record that
  // counts the number joins in a day (86400000000 microseconds)
  static const char kCopyJoinHistoryTableSql[] =
      // clang-format off
      "INSERT INTO join_history2 "
      "SELECT owner,"
             "name,"
             "(join_time-(join_time%86400000000)) as join_time2,"
             "COUNT() as count "
      "FROM join_history "
      "GROUP BY owner,name,join_time2";
  // clang-format on
  if (!db.Execute(kCopyJoinHistoryTableSql))
    return false;

  static const char kDropJoinHistoryTableSql[] = "DROP TABLE join_history";
  if (!db.Execute(kDropJoinHistoryTableSql))
    return false;

  static const char kRenameJoinHistoryTableSql[] =
      // clang-format off
      "ALTER TABLE join_history2 "
      "RENAME TO join_history";
  // clang-format on
  if (!db.Execute(kRenameJoinHistoryTableSql))
    return false;

  static const char kBidHistoryTableSql[] =
      // clang-format off
      "CREATE TABLE bid_history2("
        "owner TEXT NOT NULL,"
        "name TEXT NOT NULL,"
        "bid_time INTEGER NOT NULL,"
        "count INTEGER NOT NULL,"
      "PRIMARY KEY(owner, name, bid_time) "
      "FOREIGN KEY(owner,name) REFERENCES interest_groups)";
  // clang-format on
  if (!db.Execute(kBidHistoryTableSql))
    return false;

  // Consolidate old bid records into a single record that
  // counts the number bids in a day (86400000000 microseconds)
  static const char kCopyBidHistoryTableSql[] =
      // clang-format off
      "INSERT INTO bid_history2 "
      "SELECT owner,"
             "name,"
             "(bid_time-(bid_time%86400000000)) as bid_time2,"
             "COUNT() as count "
      "FROM bid_history "
      "GROUP BY owner,name,bid_time2";
  // clang-format on
  if (!db.Execute(kCopyBidHistoryTableSql))
    return false;

  static const char kDropBidHistoryTableSql[] = "DROP TABLE bid_history";
  if (!db.Execute(kDropBidHistoryTableSql))
    return false;

  static const char kRenameBidHistoryTableSql[] =
      // clang-format off
      "ALTER TABLE bid_history2 "
      "RENAME TO bid_history";
  // clang-format on
  if (!db.Execute(kRenameBidHistoryTableSql))
    return false;
  return true;
}

bool UpgradeV7SchemaToV8(sql::Database& db, sql::MetaTable& meta_table) {
  static const char kInterestGroupsAddExecutionModeSql[] =
      "ALTER TABLE interest_groups ADD COLUMN execution_mode INTEGER DEFAULT 0";
  if (!db.Execute(kInterestGroupsAddExecutionModeSql))
    return false;
  return true;
}

bool UpgradeV6SchemaToV7(sql::Database& db, sql::MetaTable& meta_table) {
  // Index on group expiration by owner.
  DCHECK(db.DoesIndexExist("interest_group_owner"));
  static const char kRemoveInterstGroupOwnerIndexSql[] =
      // clang-format off
      "DROP INDEX interest_group_owner";
  // clang-format on
  if (!db.Execute(kRemoveInterstGroupOwnerIndexSql))
    return false;
  DCHECK(!db.DoesIndexExist("interest_group_owner"));
  static const char kInterestGroupOwnerIndexSql[] =
      // clang-format off
      "CREATE INDEX interest_group_owner"
      " ON interest_groups(owner,expiration DESC,next_update_after ASC,name)";
  // clang-format on
  if (!db.Execute(kInterestGroupOwnerIndexSql))
    return false;

  // Update interest_group table.
  static const char kInterestGroupsAddPrioritySql[] =
      "ALTER TABLE interest_groups ADD COLUMN priority DOUBLE DEFAULT 0";
  if (!db.Execute(kInterestGroupsAddPrioritySql))
    return false;
  return true;
}

bool MaybeCreateKAnonEntry(sql::Database& db,
                           const std::string& key,
                           const base::Time& now) {
  base::Time distant_past = base::Time::Min();

  // clang-format off
  sql::Statement maybe_insert_kanon(
      db.GetCachedStatement(SQL_FROM_HERE,
          "INSERT OR IGNORE INTO k_anon("
              "last_referenced_time,"
              "key,"
              "is_k_anon,"
              "last_k_anon_updated_time,"
              "last_reported_to_anon_server_time) "
            "VALUES(?,?,0,?,?)"
      ));
  // clang-format on
  if (!maybe_insert_kanon.is_valid())
    return false;

  maybe_insert_kanon.Reset(true);
  maybe_insert_kanon.BindTime(0, now);
  maybe_insert_kanon.BindString(1, key);
  maybe_insert_kanon.BindTime(2, distant_past);
  maybe_insert_kanon.BindTime(3, distant_past);

  return maybe_insert_kanon.Run();
}

bool RemoveJoinHistory(sql::Database& db,
                       const blink::InterestGroupKey& group_key) {
  sql::Statement remove_join_history(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "DELETE FROM join_history "
                            "WHERE owner=? AND name=?"));
  if (!remove_join_history.is_valid())
    return false;

  remove_join_history.Reset(true);
  remove_join_history.BindString(0, Serialize(group_key.owner));
  remove_join_history.BindString(1, group_key.name);
  return remove_join_history.Run();
}

bool RemoveBidHistory(sql::Database& db,
                      const blink::InterestGroupKey& group_key) {
  sql::Statement remove_bid_history(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "DELETE FROM bid_history "
                            "WHERE owner=? AND name=?"));
  if (!remove_bid_history.is_valid())
    return false;

  remove_bid_history.Reset(true);
  remove_bid_history.BindString(0, Serialize(group_key.owner));
  remove_bid_history.BindString(1, group_key.name);
  return remove_bid_history.Run();
}

bool RemoveWinHistory(sql::Database& db,
                      const blink::InterestGroupKey& group_key) {
  sql::Statement remove_win_history(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "DELETE FROM win_history "
                            "WHERE owner=? AND name=?"));
  if (!remove_win_history.is_valid())
    return false;

  remove_win_history.Reset(true);
  remove_win_history.BindString(0, Serialize(group_key.owner));
  remove_win_history.BindString(1, group_key.name);
  return remove_win_history.Run();
}

bool DoRemoveInterestGroup(sql::Database& db,
                           const blink::InterestGroupKey& group_key) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin())
    return false;

  // These tables have foreign keys that reference the interest group table.
  if (!RemoveJoinHistory(db, group_key))
    return false;
  if (!RemoveBidHistory(db, group_key))
    return false;
  if (!RemoveWinHistory(db, group_key))
    return false;

  sql::Statement remove_group(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "DELETE FROM interest_groups "
                            "WHERE owner=? AND name=?"));
  if (!remove_group.is_valid())
    return false;

  remove_group.Reset(true);
  remove_group.BindString(0, Serialize(group_key.owner));
  remove_group.BindString(1, group_key.name);
  return remove_group.Run() && transaction.Commit();
}

bool DoClearClusteredBiddingGroups(sql::Database& db,
                                   const url::Origin owner,
                                   const url::Origin main_frame) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin())
    return false;

  // clang-format off
  sql::Statement same_cluster_groups(
      db.GetCachedStatement(SQL_FROM_HERE,
        "SELECT name "
        "FROM interest_groups "
        "WHERE owner = ? AND joining_origin = ? AND execution_mode = ?"));
  // clang-format on

  if (!same_cluster_groups.is_valid())
    return false;

  same_cluster_groups.Reset(true);
  same_cluster_groups.BindString(0, Serialize(owner));
  same_cluster_groups.BindString(1, Serialize(main_frame));
  same_cluster_groups.BindInt(
      2, static_cast<int>(
             blink::InterestGroup::ExecutionMode::kGroupedByOriginMode));

  while (same_cluster_groups.Step()) {
    if (!DoRemoveInterestGroup(
            db, blink::InterestGroupKey(owner,
                                        same_cluster_groups.ColumnString(0))))
      return false;
  }
  return transaction.Commit();
}

bool DoLoadInterestGroup(sql::Database& db,
                         const blink::InterestGroupKey& group_key,
                         blink::InterestGroup& group,
                         url::Origin* joining_origin = nullptr,
                         base::Time* exact_join_time = nullptr,
                         base::Time* last_updated = nullptr) {
  // clang-format off
  sql::Statement load(
      db.GetCachedStatement(SQL_FROM_HERE,
        "SELECT expiration,"
          "joining_origin,"
          "exact_join_time,"
          "last_updated,"
          "priority,"
          "enable_bidding_signals_prioritization,"
          "priority_vector,"
          "priority_signals_overrides,"
          "seller_capabilities,"
          "all_sellers_capabilities,"
          "execution_mode,"
          "bidding_url,"
          "bidding_wasm_helper_url,"
          "update_url,"
          "trusted_bidding_signals_url,"
          "trusted_bidding_signals_keys,"
          "user_bidding_signals,"  // opaque data
          "ads,"
          "ad_components,"
          "ad_sizes,"
          "size_groups "
        "FROM interest_groups "
        "WHERE owner = ? AND name = ? "));
  // clang-format on

  if (!load.is_valid())
    return false;

  load.Reset(true);
  load.BindString(0, Serialize(group_key.owner));
  load.BindString(1, group_key.name);

  if (!load.Step() || !load.Succeeded())
    return false;

  group.expiry = load.ColumnTime(0);
  group.owner = group_key.owner;
  group.name = group_key.name;
  if (joining_origin)
    *joining_origin = DeserializeOrigin(load.ColumnString(1));
  if (exact_join_time)
    *exact_join_time = load.ColumnTime(2);
  if (last_updated)
    *last_updated = load.ColumnTime(3);
  group.priority = load.ColumnDouble(4);
  group.enable_bidding_signals_prioritization = load.ColumnBool(5);
  group.priority_vector = DeserializeStringDoubleMap(load.ColumnString(6));
  group.priority_signals_overrides =
      DeserializeStringDoubleMap(load.ColumnString(7));
  group.seller_capabilities =
      DeserializeSellerCapabilitiesMap(load.ColumnString(8));
  group.all_sellers_capabilities =
      DeserializeSellerCapabilities(load.ColumnInt64(9));
  group.execution_mode =
      static_cast<blink::InterestGroup::ExecutionMode>(load.ColumnInt(10));
  group.bidding_url = DeserializeURL(load.ColumnString(11));
  group.bidding_wasm_helper_url = DeserializeURL(load.ColumnString(12));
  group.daily_update_url = DeserializeURL(load.ColumnString(13));
  group.trusted_bidding_signals_url = DeserializeURL(load.ColumnString(14));
  group.trusted_bidding_signals_keys =
      DeserializeStringVector(load.ColumnString(15));
  if (load.GetColumnType(16) != sql::ColumnType::kNull)
    group.user_bidding_signals = load.ColumnString(16);
  group.ads = DeserializeInterestGroupAdVector(load.ColumnString(17));
  group.ad_components = DeserializeInterestGroupAdVector(load.ColumnString(18));
  group.ad_sizes = DeserializeStringSizeMap(load.ColumnString(19));
  group.size_groups = DeserializeStringStringVectorMap(load.ColumnString(20));

  return true;
}

bool DoRecordInterestGroupJoin(sql::Database& db,
                               const url::Origin& owner,
                               const std::string& name,
                               base::Time join_time) {
  // This flow basically emulates SQLite's UPSERT feature which is disabled in
  // Chrome. Although there are two statements executed, we don't need to
  // enclose them in a transaction since only one will actually modify the
  // database.

  // clang-format off
  sql::Statement insert_join_hist(
      db.GetCachedStatement(SQL_FROM_HERE,
      "INSERT OR IGNORE INTO join_history(owner,name,join_time,count) "
      "VALUES(?,?,?,1)"));
  // clang-format on
  if (!insert_join_hist.is_valid())
    return false;

  insert_join_hist.Reset(true);
  insert_join_hist.BindString(0, Serialize(owner));
  insert_join_hist.BindString(1, name);
  insert_join_hist.BindTime(2, join_time);
  if (!insert_join_hist.Run())
    return false;

  // If the insert changed the database return early.
  if (db.GetLastChangeCount() > 0)
    return true;

  // clang-format off
  sql::Statement update_join_hist(
      db.GetCachedStatement(SQL_FROM_HERE,
          "UPDATE join_history "
          "SET count=count+1 "
          "WHERE owner=? AND name=? AND join_time=?"));
  // clang-format on
  if (!update_join_hist.is_valid())
    return false;

  update_join_hist.Reset(true);
  update_join_hist.BindString(0, Serialize(owner));
  update_join_hist.BindString(1, name);
  update_join_hist.BindTime(2, join_time);

  return update_join_hist.Run();
}

bool DoJoinInterestGroup(sql::Database& db,
                         const blink::InterestGroup& data,
                         const GURL& joining_url,
                         base::Time exact_join_time,
                         base::Time last_updated,
                         base::Time next_update_after) {
  DCHECK(data.IsValid());
  url::Origin joining_origin = url::Origin::Create(joining_url);
  sql::Transaction transaction(&db);
  if (!transaction.Begin())
    return false;

  blink::InterestGroup old_group;
  url::Origin old_joining_origin;
  if (DoLoadInterestGroup(db, blink::InterestGroupKey(data.owner, data.name),
                          old_group, &old_joining_origin,
                          /*exact_join_time=*/nullptr,
                          /*last_updated=*/nullptr) &&
      old_group.execution_mode ==
          blink::InterestGroup::ExecutionMode::kGroupedByOriginMode &&
      joining_origin != old_joining_origin) {
    // Clear all interest groups with same owner and mode GroupedByOriginMode
    // and same old_joining_origin.
    if (!DoClearClusteredBiddingGroups(db, data.owner, old_joining_origin))
      return false;
  }

  // clang-format off
  sql::Statement join_group(
      db.GetCachedStatement(SQL_FROM_HERE,
          "INSERT OR REPLACE INTO interest_groups("
            "expiration,"
            "last_updated,"
            "next_update_after,"
            "owner,"
            "joining_origin,"
           "exact_join_time,"
            "name,"
            "priority,"
            "enable_bidding_signals_prioritization,"
            "priority_vector,"
            "priority_signals_overrides,"
            "seller_capabilities,"
            "all_sellers_capabilities,"
            "execution_mode,"
            "joining_url,"
            "bidding_url,"
            "bidding_wasm_helper_url,"
            "update_url,"
            "trusted_bidding_signals_url,"
            "trusted_bidding_signals_keys,"
            "user_bidding_signals,"  // opaque data
            "ads,"
            "ad_components,"
            "ad_sizes,"
            "size_groups) "
          "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));

  // clang-format on
  if (!join_group.is_valid())
    return false;
  join_group.Reset(true);
  join_group.BindTime(0, data.expiry);
  join_group.BindTime(1, last_updated);
  join_group.BindTime(2, next_update_after);
  join_group.BindString(3, Serialize(data.owner));
  join_group.BindString(4, Serialize(joining_origin));
  join_group.BindTime(5, exact_join_time);
  join_group.BindString(6, data.name);
  join_group.BindDouble(7, data.priority);
  join_group.BindBool(8, data.enable_bidding_signals_prioritization);
  join_group.BindString(9, Serialize(data.priority_vector));
  join_group.BindString(10, Serialize(data.priority_signals_overrides));
  join_group.BindString(11, Serialize(data.seller_capabilities));
  join_group.BindInt64(12, Serialize(data.all_sellers_capabilities));
  join_group.BindInt(13, static_cast<int>(data.execution_mode));
  join_group.BindString(14, Serialize(joining_url));
  join_group.BindString(15, Serialize(data.bidding_url));
  join_group.BindString(16, Serialize(data.bidding_wasm_helper_url));
  join_group.BindString(17, Serialize(data.daily_update_url));
  join_group.BindString(18, Serialize(data.trusted_bidding_signals_url));
  join_group.BindString(19, Serialize(data.trusted_bidding_signals_keys));
  if (data.user_bidding_signals) {
    join_group.BindString(20, data.user_bidding_signals.value());
  } else {
    join_group.BindNull(20);
  }
  join_group.BindString(21, Serialize(data.ads));
  join_group.BindString(22, Serialize(data.ad_components));
  join_group.BindString(23, Serialize(data.ad_sizes));
  join_group.BindString(24, Serialize(data.size_groups));

  if (!join_group.Run())
    return false;

  if (!DoRecordInterestGroupJoin(db, data.owner, data.name, last_updated))
    return false;

  return transaction.Commit();
}

bool DoStoreInterestGroupUpdate(sql::Database& db,
                                const blink::InterestGroup& group,
                                base::Time now) {
  // clang-format off
  sql::Statement store_group(
      db.GetCachedStatement(SQL_FROM_HERE,
          "UPDATE interest_groups "
          "SET last_updated=?,"
            "next_update_after=?,"
            "priority=?,"
            "enable_bidding_signals_prioritization=?,"
            "priority_vector=?,"
            "priority_signals_overrides=?,"
            "seller_capabilities=?,"
            "all_sellers_capabilities=?,"
            "execution_mode=?,"
            "bidding_url=?,"
            "bidding_wasm_helper_url=?,"
            "update_url=?,"
            "trusted_bidding_signals_url=?,"
            "trusted_bidding_signals_keys=?,"
            "ads=?,"
            "ad_components=?,"
            "ad_sizes=?,"
            "size_groups=? "
          "WHERE owner=? AND name=?"));

  // clang-format on
  if (!store_group.is_valid())
    return false;

  store_group.Reset(true);
  store_group.BindTime(0, now);
  store_group.BindTime(
      1, now + InterestGroupStorage::kUpdateSucceededBackoffPeriod);
  store_group.BindDouble(2, group.priority);
  store_group.BindBool(3, group.enable_bidding_signals_prioritization);
  store_group.BindString(4, Serialize(group.priority_vector));
  store_group.BindString(5, Serialize(group.priority_signals_overrides));
  store_group.BindString(6, Serialize(group.seller_capabilities));
  store_group.BindInt64(7, Serialize(group.all_sellers_capabilities));
  store_group.BindInt(8, static_cast<int>(group.execution_mode));
  store_group.BindString(9, Serialize(group.bidding_url));
  store_group.BindString(10, Serialize(group.bidding_wasm_helper_url));
  store_group.BindString(11, Serialize(group.daily_update_url));
  store_group.BindString(12, Serialize(group.trusted_bidding_signals_url));
  store_group.BindString(13, Serialize(group.trusted_bidding_signals_keys));
  store_group.BindString(14, Serialize(group.ads));
  store_group.BindString(15, Serialize(group.ad_components));
  store_group.BindString(16, Serialize(group.ad_sizes));
  store_group.BindString(17, Serialize(group.size_groups));
  store_group.BindString(18, Serialize(group.owner));
  store_group.BindString(19, group.name);

  return store_group.Run();
}

bool DoUpdateInterestGroup(sql::Database& db,
                           const blink::InterestGroupKey& group_key,
                           InterestGroupUpdate update,
                           base::Time now) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin())
    return false;

  // Unlike Join() operations, for Update() operations, values that aren't
  // specified in the JSON returned by servers (Serialize()'d below as empty
  // strings) aren't modified in the database -- in this sense, new data is
  // merged with old data.
  //
  // Since we need to verify this results in a valid interest group, we have to
  // first read the interest group from the DB, apply the changes and then
  // verify the interest group is valid before writing it to the database.

  blink::InterestGroup stored_group;
  if (!DoLoadInterestGroup(db, group_key, stored_group,
                           /*joining_origin=*/nullptr,
                           /*exact_join_time=*/nullptr,
                           /*last_updated=*/nullptr)) {
    return false;
  }

  // (Optimization) Don't do anything for expired interest groups.
  if (stored_group.expiry < now)
    return false;
  if (update.priority)
    stored_group.priority = *update.priority;
  if (update.enable_bidding_signals_prioritization) {
    stored_group.enable_bidding_signals_prioritization =
        *update.enable_bidding_signals_prioritization;
  }
  if (update.priority_vector)
    stored_group.priority_vector = update.priority_vector;
  if (update.priority_signals_overrides) {
    MergePrioritySignalsOverrides(*update.priority_signals_overrides,
                                  stored_group.priority_signals_overrides);
  }
  if (update.seller_capabilities)
    stored_group.seller_capabilities = update.seller_capabilities;
  if (update.all_sellers_capabilities)
    stored_group.all_sellers_capabilities = *update.all_sellers_capabilities;
  if (update.execution_mode)
    stored_group.execution_mode = *update.execution_mode;
  if (update.bidding_url)
    stored_group.bidding_url = std::move(update.bidding_url);
  if (update.bidding_wasm_helper_url) {
    stored_group.bidding_wasm_helper_url =
        std::move(update.bidding_wasm_helper_url);
  }
  if (update.trusted_bidding_signals_url) {
    stored_group.trusted_bidding_signals_url =
        std::move(update.trusted_bidding_signals_url);
  }
  if (update.trusted_bidding_signals_keys) {
    stored_group.trusted_bidding_signals_keys =
        std::move(update.trusted_bidding_signals_keys);
  }
  if (update.ads)
    stored_group.ads = std::move(update.ads);
  if (update.ad_components)
    stored_group.ad_components = std::move(update.ad_components);
  if (update.ad_sizes) {
    stored_group.ad_sizes = std::move(update.ad_sizes);
  }
  if (update.size_groups) {
    stored_group.size_groups = std::move(update.size_groups);
  }

  if (!stored_group.IsValid()) {
    // TODO(behamilton): Report errors to devtools.
    return false;
  }

  if (!DoStoreInterestGroupUpdate(db, stored_group, now))
    return false;

  return transaction.Commit();
}

bool DoReportUpdateFailed(sql::Database& db,
                          const blink::InterestGroupKey& group_key,
                          bool parse_failure,
                          base::Time now) {
  sql::Statement update_group(db.GetCachedStatement(SQL_FROM_HERE, R"(
UPDATE interest_groups SET
  next_update_after=?
WHERE owner=? AND name=?)"));

  if (!update_group.is_valid())
    return false;

  update_group.Reset(true);
  if (parse_failure) {
    // Non-network failures delay the same amount of time as successful updates.
    update_group.BindTime(
        0, now + InterestGroupStorage::kUpdateSucceededBackoffPeriod);
  } else {
    update_group.BindTime(
        0, now + InterestGroupStorage::kUpdateFailedBackoffPeriod);
  }
  update_group.BindString(1, Serialize(group_key.owner));
  update_group.BindString(2, group_key.name);

  return update_group.Run();
}

bool DoRecordInterestGroupBid(sql::Database& db,
                              const blink::InterestGroupKey& group_key,
                              base::Time bid_time) {
  // This flow basically emulates SQLite's UPSERT feature which is disabled in
  // Chrome. Although there are two statements executed, we don't need to
  // enclose them in a transaction since only one will actually modify the
  // database.

  // clang-format off
  sql::Statement insert_bid_hist(
      db.GetCachedStatement(SQL_FROM_HERE,
      "INSERT OR IGNORE INTO bid_history(owner,name,bid_time,count) "
      "VALUES(?,?,?,1)"));
  // clang-format on
  if (!insert_bid_hist.is_valid())
    return false;

  insert_bid_hist.Reset(true);
  insert_bid_hist.BindString(0, Serialize(group_key.owner));
  insert_bid_hist.BindString(1, group_key.name);
  insert_bid_hist.BindTime(2, bid_time);
  if (!insert_bid_hist.Run())
    return false;

  // If the insert changed the database return early.
  if (db.GetLastChangeCount() > 0)
    return true;

  // clang-format off
  sql::Statement update_bid_hist(
      db.GetCachedStatement(SQL_FROM_HERE,
          "UPDATE bid_history "
          "SET count=count+1 "
          "WHERE owner=? AND name=? AND bid_time=?"));
  // clang-format on
  if (!update_bid_hist.is_valid())
    return false;

  update_bid_hist.Reset(true);
  update_bid_hist.BindString(0, Serialize(group_key.owner));
  update_bid_hist.BindString(1, group_key.name);
  update_bid_hist.BindTime(2, bid_time);

  return update_bid_hist.Run();
}

bool DoRecordInterestGroupBids(sql::Database& db,
                               const blink::InterestGroupSet& group_keys,
                               base::Time bid_time) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin())
    return false;

  for (const auto& group_key : group_keys) {
    if (!DoRecordInterestGroupBid(db, group_key, bid_time))
      return false;
  }
  return transaction.Commit();
}

bool DoRecordInterestGroupWin(sql::Database& db,
                              const blink::InterestGroupKey& group_key,
                              const std::string& ad_json,
                              base::Time win_time) {
  // Record the win. It should be unique since auctions should be serialized.
  // If it is not unique we should just keep the first one.
  // clang-format off
  sql::Statement win_hist(
      db.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO win_history(owner,name,win_time,ad) "
      "VALUES(?,?,?,?)"));
  // clang-format on
  if (!win_hist.is_valid())
    return false;

  win_hist.Reset(true);
  win_hist.BindString(0, Serialize(group_key.owner));
  win_hist.BindString(1, group_key.name);
  win_hist.BindTime(2, win_time);
  win_hist.BindString(3, ad_json);
  return win_hist.Run();
}

bool DoUpdateKAnonymity(sql::Database& db,
                        const StorageInterestGroup::KAnonymityData& data,
                        base::Time now) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin())
    return false;

  if (!MaybeCreateKAnonEntry(db, data.key, now))
    return false;

  // clang-format off
  sql::Statement update(
      db.GetCachedStatement(SQL_FROM_HERE,
      "UPDATE k_anon "
      "SET is_k_anon=?,"
          "last_k_anon_updated_time=?,"
          "last_referenced_time=? "
      "WHERE key=?"));
  // clang-format on
  if (!update.is_valid())
    return false;

  update.Reset(true);
  update.BindInt(0, data.is_k_anonymous);
  update.BindTime(1, data.last_updated);
  update.BindTime(2, now);
  update.BindString(3, data.key);
  if (!update.Run())
    return false;
  return transaction.Commit();
}

absl::optional<base::Time> DoGetLastKAnonymityReported(sql::Database& db,
                                                       const std::string& key) {
  const base::Time distant_past = base::Time::Min();

  sql::Statement get_reported(db.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT last_reported_to_anon_server_time FROM k_anon WHERE key=?"));
  if (!get_reported.is_valid()) {
    DLOG(ERROR) << "GetLastKAnonymityReported SQL statement did not compile: "
                << db.GetErrorMessage();
    return absl::nullopt;
  }
  get_reported.Reset(true);
  get_reported.BindString(0, key);
  if (!get_reported.Step()) {
    return distant_past;
  }
  if (!get_reported.Succeeded())
    return absl::nullopt;
  return get_reported.ColumnTime(0);
}

void DoUpdateLastKAnonymityReported(sql::Database& db,
                                    const std::string& key,
                                    base::Time now) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin())
    return;

  if (!MaybeCreateKAnonEntry(db, key, now))
    return;

  // clang-format off
  sql::Statement set_reported(db.GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE k_anon "
      "SET last_reported_to_anon_server_time=?,"
          "last_referenced_time=? "
      "WHERE key=?"));
  // clang-format on
  if (!set_reported.is_valid()) {
    DLOG(ERROR)
        << "DoUpdateLastKAnonymityReported SQL statement did not compile: "
        << db.GetErrorMessage();
    return;
  }
  set_reported.Reset(true);
  set_reported.BindTime(0, now);
  set_reported.BindTime(1, now);
  set_reported.BindString(2, key);
  if (!set_reported.Run()) {
    return;
  }
  transaction.Commit();
}

absl::optional<std::vector<url::Origin>> DoGetAllInterestGroupOwners(
    sql::Database& db,
    base::Time expiring_after) {
  std::vector<url::Origin> result;
  sql::Statement load(db.GetCachedStatement(SQL_FROM_HERE,
                                            "SELECT DISTINCT owner "
                                            "FROM interest_groups "
                                            "WHERE expiration>=? "
                                            "ORDER BY expiration DESC"));
  if (!load.is_valid()) {
    DLOG(ERROR) << "LoadAllInterestGroups SQL statement did not compile: "
                << db.GetErrorMessage();
    return absl::nullopt;
  }
  load.Reset(true);
  load.BindTime(0, expiring_after);
  while (load.Step()) {
    result.push_back(DeserializeOrigin(load.ColumnString(0)));
  }
  if (!load.Succeeded())
    return absl::nullopt;
  return result;
}

absl::optional<std::vector<url::Origin>> DoGetAllInterestGroupJoiningOrigins(
    sql::Database& db,
    base::Time expiring_after) {
  std::vector<url::Origin> result;
  sql::Statement load(db.GetCachedStatement(SQL_FROM_HERE,
                                            "SELECT DISTINCT joining_origin "
                                            "FROM interest_groups "
                                            "WHERE expiration>=?"));
  if (!load.is_valid()) {
    DLOG(ERROR)
        << "LoadAllInterestGroupJoiningOrigins SQL statement did not compile: "
        << db.GetErrorMessage();
    return absl::nullopt;
  }
  load.Reset(true);
  load.BindTime(0, expiring_after);
  while (load.Step()) {
    result.push_back(DeserializeOrigin(load.ColumnString(0)));
  }
  if (!load.Succeeded())
    return absl::nullopt;
  return result;
}

bool DoRemoveInterestGroupsMatchingOwnerAndJoiner(sql::Database& db,
                                                  url::Origin owner,
                                                  url::Origin joining_origin,
                                                  base::Time expiring_after) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin())
    return false;

  std::vector<std::string> owner_joiner_names;
  sql::Statement load(db.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT name "
      "FROM interest_groups "
      "WHERE owner=? AND joining_origin=? AND expiration>=?"));

  if (!load.is_valid())
    return false;

  load.Reset(true);
  load.BindString(0, owner.Serialize());
  load.BindString(1, joining_origin.Serialize());
  load.BindTime(2, expiring_after);

  while (load.Step()) {
    owner_joiner_names.emplace_back(load.ColumnString(0));
  }

  for (const auto& name : owner_joiner_names) {
    if (!DoRemoveInterestGroup(db, blink::InterestGroupKey{owner, name}))
      return false;
  }

  return transaction.Commit();
}

absl::optional<std::vector<std::pair<url::Origin, url::Origin>>>
DoGetAllInterestGroupOwnerJoinerPairs(sql::Database& db,
                                      base::Time expiring_after) {
  std::vector<std::pair<url::Origin, url::Origin>> result;
  sql::Statement load(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "SELECT DISTINCT owner,joining_origin "
                            "FROM interest_groups "
                            "WHERE expiration>=?"));
  if (!load.is_valid()) {
    DLOG(ERROR) << "LoadAllInterestGroupOwnerJoinerPairs SQL statement did not "
                   "compile: "
                << db.GetErrorMessage();
    return absl::nullopt;
  }
  load.Reset(true);
  load.BindTime(0, expiring_after);
  while (load.Step()) {
    result.emplace_back(DeserializeOrigin(load.ColumnString(0)),
                        DeserializeOrigin(load.ColumnString(1)));
  }
  if (!load.Succeeded())
    return absl::nullopt;
  return result;
}

bool DoGetKAnonymity(
    sql::Database& db,
    const std::string& key,
    absl::optional<StorageInterestGroup::KAnonymityData>& output) {
  // clang-format off
  sql::Statement interest_group_kanon(
    db.GetCachedStatement(SQL_FROM_HERE,
      "SELECT is_k_anon, last_k_anon_updated_time "
      "FROM k_anon "
      "WHERE key=?"
    ));
  // clang-format on
  if (!interest_group_kanon.is_valid()) {
    DLOG(ERROR)
        << "GetInterestGroupsForOwner interest_group_kanon SQL statement did "
           "not compile: "
        << db.GetErrorMessage();
    return false;
  }

  interest_group_kanon.Reset(true);
  interest_group_kanon.BindString(0, key);

  if (!interest_group_kanon.Step()) {
    // Not in the table, so return the defaults.
    output = DefaultKAnonymityData(key);
    return true;
  }

  output = {key, /*is_k_anonymous=*/interest_group_kanon.ColumnInt(0) > 0,
            /*last_updated=*/interest_group_kanon.ColumnTime(1)};
  return interest_group_kanon.Succeeded();
}

bool GetPreviousWins(sql::Database& db,
                     const blink::InterestGroupKey& group_key,
                     base::Time win_time_after,
                     BiddingBrowserSignalsPtr& output) {
  // clang-format off
  sql::Statement prev_wins(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "SELECT win_time, ad "
                            "FROM win_history "
                            "WHERE owner = ? AND name = ? AND win_time >= ? "
                            "ORDER BY win_time DESC"));
  // clang-format on
  if (!prev_wins.is_valid()) {
    DLOG(ERROR) << "GetInterestGroupsForOwner win_history SQL statement did "
                   "not compile: "
                << db.GetErrorMessage();
    return false;
  }
  prev_wins.Reset(true);
  prev_wins.BindString(0, Serialize(group_key.owner));
  prev_wins.BindString(1, group_key.name);
  prev_wins.BindTime(2, win_time_after);
  while (prev_wins.Step()) {
    PreviousWinPtr prev_win = auction_worklet::mojom::PreviousWin::New(
        /*time=*/prev_wins.ColumnTime(0),
        /*ad_json=*/prev_wins.ColumnString(1));
    output->prev_wins.push_back(std::move(prev_win));
  }
  return prev_wins.Succeeded();
}

bool GetJoinCount(sql::Database& db,
                  const blink::InterestGroupKey& group_key,
                  base::Time joined_after,
                  BiddingBrowserSignalsPtr& output) {
  // clang-format off
  sql::Statement join_count(
      db.GetCachedStatement(SQL_FROM_HERE,
    "SELECT SUM(count) "
    "FROM join_history "
    "WHERE owner = ? AND name = ? AND join_time >=?"));
  // clang-format on
  if (!join_count.is_valid()) {
    DLOG(ERROR) << "GetJoinCount SQL statement did not compile: "
                << db.GetErrorMessage();
    return false;
  }
  join_count.Reset(true);
  join_count.BindString(0, Serialize(group_key.owner));
  join_count.BindString(1, group_key.name);
  join_count.BindTime(2, joined_after);
  while (join_count.Step()) {
    output->join_count = join_count.ColumnInt64(0);
  }
  return join_count.Succeeded();
}

bool GetBidCount(sql::Database& db,
                 const blink::InterestGroupKey& group_key,
                 base::Time now,
                 BiddingBrowserSignalsPtr& output) {
  // clang-format off
  sql::Statement bid_count(
      db.GetCachedStatement(SQL_FROM_HERE,
    "SELECT SUM(count) "
    "FROM bid_history "
    "WHERE owner = ? AND name = ? AND bid_time >= ?"));
  // clang-format on
  if (!bid_count.is_valid()) {
    DLOG(ERROR) << "GetBidCount SQL statement did not compile: "
                << db.GetErrorMessage();
    return false;
  }
  bid_count.Reset(true);
  bid_count.BindString(0, Serialize(group_key.owner));
  bid_count.BindString(1, group_key.name);
  bid_count.BindTime(2, now - InterestGroupStorage::kHistoryLength);
  while (bid_count.Step()) {
    output->bid_count = bid_count.ColumnInt64(0);
  }
  return bid_count.Succeeded();
}

absl::optional<std::vector<std::string>> DoGetInterestGroupNamesForOwner(
    sql::Database& db,
    const url::Origin& owner,
    base::Time now,
    base::Time next_update_after) {
  // clang-format off
  sql::Statement get_names(
    db.GetCachedStatement(SQL_FROM_HERE,
    "SELECT name "
    "FROM interest_groups "
    "WHERE owner=? AND expiration>=? AND ?>=next_update_after "
    "ORDER BY expiration DESC"));
  // clang-format on

  if (!get_names.is_valid())
    return absl::nullopt;

  get_names.Reset(true);
  get_names.BindString(0, Serialize(owner));
  get_names.BindTime(1, now);
  get_names.BindTime(2, next_update_after);

  std::vector<std::string> result;
  while (get_names.Step()) {
    result.push_back(get_names.ColumnString(0));
  }
  if (!get_names.Succeeded())
    return absl::nullopt;

  return result;
}

absl::optional<StorageInterestGroup> DoGetStoredInterestGroup(
    sql::Database& db,
    const blink::InterestGroupKey& group_key,
    base::Time now) {
  StorageInterestGroup db_interest_group;
  if (!DoLoadInterestGroup(db, group_key, db_interest_group.interest_group,
                           &db_interest_group.joining_origin,
                           &db_interest_group.join_time,
                           &db_interest_group.last_updated)) {
    return absl::nullopt;
  }

  if (db_interest_group.interest_group.bidding_url) {
    if (db_interest_group.interest_group.ads) {
      for (auto& ad : db_interest_group.interest_group.ads.value()) {
        absl::optional<StorageInterestGroup::KAnonymityData> ad_kanon;
        if (!DoGetKAnonymity(
                db,
                blink::KAnonKeyForAdBid(db_interest_group.interest_group,
                                        ad.render_url),
                ad_kanon)) {
          return absl::nullopt;
        }
        if (!ad_kanon)
          continue;
        db_interest_group.bidding_ads_kanon.push_back(
            std::move(ad_kanon).value());

        absl::optional<StorageInterestGroup::KAnonymityData> ad_name_kanon;
        if (!DoGetKAnonymity(db,
                             blink::KAnonKeyForAdNameReporting(
                                 db_interest_group.interest_group, ad),
                             ad_name_kanon)) {
          return absl::nullopt;
        }
        if (!ad_name_kanon)
          continue;
        db_interest_group.reporting_ads_kanon.push_back(
            std::move(ad_name_kanon).value());
      }
    }
    if (db_interest_group.interest_group.ad_components) {
      for (auto& ad : db_interest_group.interest_group.ad_components.value()) {
        absl::optional<StorageInterestGroup::KAnonymityData> ad_kanon;
        if (!DoGetKAnonymity(db,
                             blink::KAnonKeyForAdComponentBid(ad.render_url),
                             ad_kanon)) {
          return absl::nullopt;
        }
        if (!ad_kanon)
          continue;
        db_interest_group.component_ads_kanon.push_back(
            std::move(ad_kanon).value());
      }
    }
  }

  db_interest_group.bidding_browser_signals =
      auction_worklet::mojom::BiddingBrowserSignals::New();
  if (!GetJoinCount(db, group_key, now - InterestGroupStorage::kHistoryLength,
                    db_interest_group.bidding_browser_signals)) {
    return absl::nullopt;
  }
  if (!GetBidCount(db, group_key, now - InterestGroupStorage::kHistoryLength,
                   db_interest_group.bidding_browser_signals)) {
    return absl::nullopt;
  }
  if (!GetPreviousWins(db, group_key,
                       now - InterestGroupStorage::kHistoryLength,
                       db_interest_group.bidding_browser_signals)) {
    return absl::nullopt;
  }
  return db_interest_group;
}

absl::optional<std::vector<StorageInterestGroup>> DoGetInterestGroupsForOwner(
    sql::Database& db,
    const url::Origin& owner,
    base::Time now,
    bool get_groups_for_update = false) {
  sql::Transaction transaction(&db);

  if (!transaction.Begin())
    return absl::nullopt;

  base::Time next_update_after =
      (get_groups_for_update ? now : base::Time::Max());
  absl::optional<std::vector<std::string>> group_names =
      DoGetInterestGroupNamesForOwner(db, owner, now, next_update_after);

  if (!group_names)
    return absl::nullopt;

  std::vector<StorageInterestGroup> result;
  for (const std::string& name : *group_names) {
    absl::optional<StorageInterestGroup> db_interest_group =
        DoGetStoredInterestGroup(db, blink::InterestGroupKey(owner, name), now);
    if (!db_interest_group)
      return absl::nullopt;
    result.push_back(std::move(db_interest_group).value());
  }

  if (!transaction.Commit())
    return absl::nullopt;

  return result;
}

absl::optional<std::vector<blink::InterestGroupKey>>
DoGetInterestGroupNamesForJoiningOrigin(sql::Database& db,
                                        const url::Origin& joining_origin,
                                        base::Time now) {
  std::vector<blink::InterestGroupKey> result;

  // clang-format off
  sql::Statement load(
      db.GetCachedStatement(SQL_FROM_HERE,
        "SELECT owner,name "
        "FROM interest_groups "
        "WHERE joining_origin = ? AND expiration >=?"));
  // clang-format on

  if (!load.is_valid()) {
    DLOG(ERROR) << "GetInterestGroupNamesForJoiningOrigin SQL statement did "
                   "not compile: "
                << db.GetErrorMessage();
    return absl::nullopt;
  }

  load.Reset(true);
  load.BindString(0, Serialize(joining_origin));
  load.BindTime(1, now);

  while (load.Step()) {
    result.emplace_back(DeserializeOrigin(load.ColumnString(0)),
                        load.ColumnString(1));
  }
  if (!load.Succeeded())
    return absl::nullopt;
  return result;
}

bool DoDeleteInterestGroupData(
    sql::Database& db,
    StoragePartition::StorageKeyMatcherFunction storage_key_matcher) {
  const base::Time distant_past = base::Time::Min();
  const base::Time distant_future = base::Time::Max();
  sql::Transaction transaction(&db);

  if (!transaction.Begin())
    return false;

  std::vector<url::Origin> affected_origins;
  absl::optional<std::vector<url::Origin>> maybe_all_origins =
      DoGetAllInterestGroupOwners(db, distant_past);

  if (!maybe_all_origins)
    return false;
  for (const url::Origin& origin : maybe_all_origins.value()) {
    if (storage_key_matcher.is_null() ||
        storage_key_matcher.Run(blink::StorageKey::CreateFirstParty(origin))) {
      affected_origins.push_back(origin);
    }
  }

  for (const auto& affected_origin : affected_origins) {
    absl::optional<std::vector<std::string>> maybe_group_names =
        DoGetInterestGroupNamesForOwner(db, affected_origin, distant_past,
                                        distant_future);
    if (!maybe_group_names)
      return false;
    for (const auto& group_name : maybe_group_names.value()) {
      if (!DoRemoveInterestGroup(
              db, blink::InterestGroupKey(affected_origin, group_name)))
        return false;
    }
  }

  affected_origins.clear();
  maybe_all_origins = DoGetAllInterestGroupJoiningOrigins(db, distant_past);
  if (!maybe_all_origins)
    return false;
  for (const url::Origin& origin : maybe_all_origins.value()) {
    if (storage_key_matcher.is_null() ||
        storage_key_matcher.Run(blink::StorageKey::CreateFirstParty(origin))) {
      affected_origins.push_back(origin);
    }
  }
  for (const auto& affected_origin : affected_origins) {
    absl::optional<std::vector<blink::InterestGroupKey>> maybe_group_names =
        DoGetInterestGroupNamesForJoiningOrigin(db, affected_origin,
                                                distant_past);
    if (!maybe_group_names)
      return false;
    for (const auto& interest_group_key : maybe_group_names.value()) {
      if (!DoRemoveInterestGroup(db, interest_group_key)) {
        return false;
      }
    }
  }

  return transaction.Commit();
}

bool DoSetInterestGroupPriority(sql::Database& db,
                                const blink::InterestGroupKey& group_key,
                                double priority) {
  // clang-format off
  sql::Statement set_priority_sql(
      db.GetCachedStatement(SQL_FROM_HERE,
          "UPDATE interest_groups "
          "SET priority=? "
          "WHERE owner=? AND name=?"));
  // clang-format on
  if (!set_priority_sql.is_valid()) {
    DLOG(ERROR) << "SetPriority SQL statement did not compile.";
    return false;
  }
  set_priority_sql.Reset(true);
  set_priority_sql.BindDouble(0, priority);
  set_priority_sql.BindString(1, Serialize(group_key.owner));
  set_priority_sql.BindString(2, group_key.name);
  return set_priority_sql.Run();
}

bool DoSetInterestGroupPrioritySignalsOverrides(
    sql::Database& db,
    const blink::InterestGroupKey& group_key,
    const absl::optional<base::flat_map<std::string, double>>&
        priority_signals_overrides) {
  // clang-format off
  sql::Statement update_priority_signals_overrides_sql(
      db.GetCachedStatement(SQL_FROM_HERE,
          "UPDATE interest_groups "
          "SET priority_signals_overrides=? "
          "WHERE owner=? AND name=?"));
  // clang-format on
  if (!update_priority_signals_overrides_sql.is_valid()) {
    DLOG(ERROR) << "SetPrioritySignalsOverrides SQL statement did not compile.";
    return false;
  }
  update_priority_signals_overrides_sql.Reset(true);
  update_priority_signals_overrides_sql.BindString(
      0, Serialize(priority_signals_overrides));
  update_priority_signals_overrides_sql.BindString(1,
                                                   Serialize(group_key.owner));
  update_priority_signals_overrides_sql.BindString(2, group_key.name);
  return update_priority_signals_overrides_sql.Run();
}

bool DeleteOldJoins(sql::Database& db, base::Time cutoff) {
  sql::Statement del_join_history(db.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM join_history WHERE join_time <= ?"));
  if (!del_join_history.is_valid()) {
    DLOG(ERROR) << "DeleteOldJoins SQL statement did not compile.";
    return false;
  }
  del_join_history.Reset(true);
  del_join_history.BindTime(0, cutoff);
  if (!del_join_history.Run()) {
    DLOG(ERROR) << "Could not delete old join_history.";
    return false;
  }
  return true;
}

bool DeleteOldBids(sql::Database& db, base::Time cutoff) {
  sql::Statement del_bid_history(db.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM bid_history WHERE bid_time <= ?"));
  if (!del_bid_history.is_valid()) {
    DLOG(ERROR) << "DeleteOldBids SQL statement did not compile.";
    return false;
  }
  del_bid_history.Reset(true);
  del_bid_history.BindTime(0, cutoff);
  if (!del_bid_history.Run()) {
    DLOG(ERROR) << "Could not delete old bid_history.";
    return false;
  }
  return true;
}

bool DeleteOldWins(sql::Database& db, base::Time cutoff) {
  sql::Statement del_win_history(db.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM win_history WHERE win_time <= ?"));
  if (!del_win_history.is_valid()) {
    DLOG(ERROR) << "DeleteOldWins SQL statement did not compile.";
    return false;
  }
  del_win_history.Reset(true);
  del_win_history.BindTime(0, cutoff);
  if (!del_win_history.Run()) {
    DLOG(ERROR) << "Could not delete old win_history.";
    return false;
  }
  return true;
}

bool ClearExcessInterestGroups(sql::Database& db,
                               size_t max_owners,
                               size_t max_owner_interest_groups) {
  const base::Time distant_past = base::Time::Min();
  const base::Time distant_future = base::Time::Max();
  const absl::optional<std::vector<url::Origin>> maybe_all_origins =
      DoGetAllInterestGroupOwners(db, distant_past);
  if (!maybe_all_origins)
    return false;
  for (size_t owner_idx = 0; owner_idx < maybe_all_origins.value().size();
       owner_idx++) {
    const url::Origin& affected_origin = maybe_all_origins.value()[owner_idx];
    const absl::optional<std::vector<std::string>> maybe_interest_groups =
        DoGetInterestGroupNamesForOwner(db, affected_origin, distant_past,
                                        distant_future);
    if (!maybe_interest_groups)
      return false;
    size_t first_idx = max_owner_interest_groups;
    if (owner_idx >= max_owners)
      first_idx = 0;
    for (size_t group_idx = first_idx;
         group_idx < maybe_interest_groups.value().size(); group_idx++) {
      if (!DoRemoveInterestGroup(
              db,
              blink::InterestGroupKey(
                  affected_origin, maybe_interest_groups.value()[group_idx]))) {
        return false;
      }
    }
  }
  return true;
}

bool ClearExpiredInterestGroups(sql::Database& db,
                                base::Time expiration_before) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin())
    return false;

  sql::Statement expired_interest_group(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "SELECT owner, name "
                            "FROM interest_groups "
                            "WHERE expiration <= ?"));
  if (!expired_interest_group.is_valid()) {
    DLOG(ERROR) << "ClearExpiredInterestGroups SQL statement did not compile.";
    return false;
  }

  expired_interest_group.Reset(true);
  expired_interest_group.BindTime(0, expiration_before);
  std::vector<blink::InterestGroupKey> expired_groups;
  while (expired_interest_group.Step()) {
    expired_groups.emplace_back(
        DeserializeOrigin(expired_interest_group.ColumnString(0)),
        expired_interest_group.ColumnString(1));
  }
  if (!expired_interest_group.Succeeded()) {
    DLOG(ERROR) << "ClearExpiredInterestGroups could not get expired groups.";
    // Keep going so we can clear any groups that we did get.
  }
  for (const auto& interest_group : expired_groups) {
    if (!DoRemoveInterestGroup(db, interest_group))
      return false;
  }
  return transaction.Commit();
}

bool ClearExpiredKAnon(sql::Database& db, base::Time cutoff) {
  sql::Statement expired_kanon(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "DELETE FROM k_anon "
                            "WHERE last_referenced_time <= ?"));
  if (!expired_kanon.is_valid()) {
    DLOG(ERROR) << "ClearExpiredKAnon SQL statement did not compile.";
    return false;
  }

  expired_kanon.Reset(true);
  expired_kanon.BindTime(0, cutoff);
  return expired_kanon.Run();
}

bool DoPerformDatabaseMaintenance(sql::Database& db,
                                  base::Time now,
                                  size_t max_owners,
                                  size_t max_owner_interest_groups) {
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS("Storage.InterestGroup.DBMaintenanceTime");
  sql::Transaction transaction(&db);
  if (!transaction.Begin())
    return false;
  if (!ClearExcessInterestGroups(db, max_owners, max_owner_interest_groups))
    return false;
  if (!ClearExpiredInterestGroups(db, now))
    return false;
  if (!DeleteOldJoins(db, now - InterestGroupStorage::kHistoryLength))
    return false;
  if (!DeleteOldBids(db, now - InterestGroupStorage::kHistoryLength))
    return false;
  if (!DeleteOldWins(db, now - InterestGroupStorage::kHistoryLength))
    return false;
  if (!ClearExpiredKAnon(db, now - InterestGroupStorage::kHistoryLength))
    return false;
  return transaction.Commit();
}

base::FilePath DBPath(const base::FilePath& base) {
  if (base.empty())
    return base;
  return base.Append(kDatabasePath);
}

}  // namespace

constexpr base::TimeDelta InterestGroupStorage::kHistoryLength;
constexpr base::TimeDelta InterestGroupStorage::kMaintenanceInterval;
constexpr base::TimeDelta InterestGroupStorage::kIdlePeriod;
constexpr base::TimeDelta InterestGroupStorage::kUpdateSucceededBackoffPeriod;
constexpr base::TimeDelta InterestGroupStorage::kUpdateFailedBackoffPeriod;

InterestGroupStorage::InterestGroupStorage(const base::FilePath& path)
    : path_to_database_(DBPath(path)),
      max_owners_(blink::features::kInterestGroupStorageMaxOwners.Get()),
      max_owner_interest_groups_(
          blink::features::kInterestGroupStorageMaxGroupsPerOwner.Get()),
      max_ops_before_maintenance_(
          blink::features::kInterestGroupStorageMaxOpsBeforeMaintenance.Get()),
      db_(std::make_unique<sql::Database>(sql::DatabaseOptions{})),
      db_maintenance_timer_(FROM_HERE,
                            kIdlePeriod,
                            this,
                            &InterestGroupStorage::PerformDBMaintenance) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

InterestGroupStorage::~InterestGroupStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool InterestGroupStorage::EnsureDBInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Time now = base::Time::Now();
  if (now > last_maintenance_time_ + kMaintenanceInterval) {
    // Schedule maintenance for next idle period. If maintenance already
    // scheduled this delays it further (we're not idle).
    db_maintenance_timer_.Reset();
  }
  // Force maintenance even if we're busy if the database may have changed too
  // much.
  if (ops_since_last_maintenance_++ > max_ops_before_maintenance_)
    PerformDBMaintenance();

  last_access_time_ = now;
  if (db_ && db_->is_open())
    return true;
  return InitializeDB();
}

bool InterestGroupStorage::InitializeDB() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  db_ = std::make_unique<sql::Database>(sql::DatabaseOptions{});
  db_->set_error_callback(base::BindRepeating(
      &InterestGroupStorage::DatabaseErrorCallback, base::Unretained(this)));
  db_->set_histogram_tag("InterestGroups");

  if (path_to_database_.empty()) {
    if (!db_->OpenInMemory()) {
      DLOG(ERROR) << "Failed to create in-memory interest group database: "
                  << db_->GetErrorMessage();
      return false;
    }
  } else {
    const base::FilePath dir = path_to_database_.DirName();

    if (!base::DirectoryExists(dir) && !base::CreateDirectory(dir)) {
      DLOG(ERROR) << "Failed to create directory for interest group database";
      return false;
    }
    if (db_->Open(path_to_database_) == false) {
      DLOG(ERROR) << "Failed to open interest group database: "
                  << db_->GetErrorMessage();
      return false;
    }
  }

  if (!InitializeSchema()) {
    db_->Close();
    return false;
  }

  DCHECK(sql::MetaTable::DoesTableExist(db_.get()));
  DCHECK(db_->DoesTableExist("interest_groups"));
  DCHECK(db_->DoesTableExist("join_history"));
  DCHECK(db_->DoesTableExist("bid_history"));
  DCHECK(db_->DoesTableExist("win_history"));
  DCHECK(db_->DoesTableExist("k_anon"));
  return true;
}

bool InterestGroupStorage::InitializeSchema() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return false;

  sql::MetaTable::RazeIfIncompatible(
      db_.get(), /*lowest_supported_version=*/kDeprecatedVersionNumber + 1,
      kCurrentVersionNumber);

  sql::MetaTable meta_table;
  bool has_metatable = meta_table.DoesTableExist(db_.get());
  if (!has_metatable && db_->DoesTableExist("interest_groups")) {
    // Existing DB with no meta table. We have no idea what version the schema
    // is so we should remove it and start fresh.
    db_->Raze();
  }
  const bool new_db = !has_metatable;
  if (!meta_table.Init(db_.get(), kCurrentVersionNumber,
                       kCompatibleVersionNumber))
    return false;

  if (new_db)
    return CreateV13Schema(*db_);

  const int db_version = meta_table.GetVersionNumber();

  if (db_version >= kCurrentVersionNumber) {
    // Getting past RazeIfIncompatible implies that
    // kCurrentVersionNumber >= meta_table.GetCompatibleVersionNumber
    // So DB is either the current database version or a future version that is
    // back-compatible with this version of Chrome.
    return true;
  }

  // Older versions - should be migrated.
  // db_version < kCurrentVersionNumber
  // db_version > kDeprecatedVersionNumber
  {
    sql::Transaction transaction(db_.get());
    if (!transaction.Begin())
      return false;
    switch (db_version) {
      case 6:
        if (!UpgradeV6SchemaToV7(*db_, meta_table))
          return false;
        ABSL_FALLTHROUGH_INTENDED;
      case 7:
        if (!UpgradeV7SchemaToV8(*db_, meta_table))
          return false;
        ABSL_FALLTHROUGH_INTENDED;
      case 8:
        if (!UpgradeV8SchemaToV9(*db_, meta_table))
          return false;
        ABSL_FALLTHROUGH_INTENDED;
      case 9:
        if (!UpgradeV9SchemaToV10(*db_, meta_table))
          return false;
        ABSL_FALLTHROUGH_INTENDED;
      case 10:
        if (!UpgradeV10SchemaToV11(*db_, meta_table))
          return false;
        ABSL_FALLTHROUGH_INTENDED;
      case 11:
        if (!UpgradeV11SchemaToV12(*db_, meta_table))
          return false;
        ABSL_FALLTHROUGH_INTENDED;
      case 12:
        if (!UpgradeV12SchemaToV13(*db_, meta_table)) {
          return false;
        }

        if (!meta_table.SetVersionNumber(kCurrentVersionNumber)) {
          return false;
        }
    }
    return transaction.Commit();
  }

  NOTREACHED();  // Only V6 through V12 should have passed RazeIfIncompatible.
  return false;
}

void InterestGroupStorage::JoinInterestGroup(
    const blink::InterestGroup& group,
    const GURL& main_frame_joining_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return;
  base::Time now = base::Time::Now();
  if (!DoJoinInterestGroup(*db_, group, main_frame_joining_url,
                           /*exact_join_time=*/now,
                           /*last_updated=*/now,
                           /*next_update_after=*/base::Time::Min()))
    DLOG(ERROR) << "Could not join interest group: " << db_->GetErrorMessage();
}

void InterestGroupStorage::LeaveInterestGroup(
    const blink::InterestGroupKey& group_key,
    const url::Origin& main_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return;

  blink::InterestGroup old_group;
  url::Origin old_joining_origin;
  if (DoLoadInterestGroup(*db_, group_key, old_group, &old_joining_origin,
                          /*exact_join_time=*/nullptr,
                          /*last_updated=*/nullptr) &&
      old_group.execution_mode ==
          blink::InterestGroup::ExecutionMode::kGroupedByOriginMode &&
      main_frame != old_joining_origin) {
    // Clear all interest groups with same owner and mode GroupedByOriginMode
    // and same old_joining_origin.
    if (!DoClearClusteredBiddingGroups(*db_, group_key.owner,
                                       old_joining_origin))
      DLOG(ERROR) << "Could not leave interest group: "
                  << db_->GetErrorMessage();
    return;
  }

  if (!DoRemoveInterestGroup(*db_, group_key))
    DLOG(ERROR) << "Could not leave interest group: " << db_->GetErrorMessage();
}

bool InterestGroupStorage::UpdateInterestGroup(
    const blink::InterestGroupKey& group_key,
    InterestGroupUpdate update) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return false;

  bool success =
      DoUpdateInterestGroup(*db_, group_key, update, base::Time::Now());
  if (!success) {
    DLOG(ERROR) << "Could not update interest group: "
                << db_->GetErrorMessage();
  }
  return success;
}

void InterestGroupStorage::ReportUpdateFailed(
    const blink::InterestGroupKey& group_key,
    bool parse_failure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized()) {
    NOTREACHED();  // We already fetched interest groups to update...
    return;
  }

  if (!DoReportUpdateFailed(*db_, group_key, parse_failure,
                            base::Time::Now())) {
    DLOG(ERROR) << "Couldn't update next_update_after: "
                << db_->GetErrorMessage();
  }
}

void InterestGroupStorage::RecordInterestGroupBids(
    const blink::InterestGroupSet& group_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return;

  if (!DoRecordInterestGroupBids(*db_, group_keys, base::Time::Now())) {
    DLOG(ERROR) << "Could not record win for interest group: "
                << db_->GetErrorMessage();
  }
}

void InterestGroupStorage::RecordInterestGroupWin(
    const blink::InterestGroupKey& group_key,
    const std::string& ad_json) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return;

  if (!DoRecordInterestGroupWin(*db_, group_key, ad_json, base::Time::Now())) {
    DLOG(ERROR) << "Could not record bid for interest group: "
                << db_->GetErrorMessage();
  }
}

void InterestGroupStorage::UpdateKAnonymity(
    const StorageInterestGroup::KAnonymityData& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return;

  if (!DoUpdateKAnonymity(*db_, data, base::Time::Now())) {
    DLOG(ERROR) << "Could not update k-anonymity: " << db_->GetErrorMessage();
  }
}

absl::optional<base::Time> InterestGroupStorage::GetLastKAnonymityReported(
    const std::string& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return {};

  return DoGetLastKAnonymityReported(*db_, key);
}

void InterestGroupStorage::UpdateLastKAnonymityReported(
    const std::string& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return;

  DoUpdateLastKAnonymityReported(*db_, key, base::Time::Now());
}

absl::optional<StorageInterestGroup> InterestGroupStorage::GetInterestGroup(
    const blink::InterestGroupKey& group_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return absl::nullopt;

  return DoGetStoredInterestGroup(*db_, group_key, base::Time::Now());
}

std::vector<url::Origin> InterestGroupStorage::GetAllInterestGroupOwners() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return {};

  absl::optional<std::vector<url::Origin>> maybe_result =
      DoGetAllInterestGroupOwners(*db_, base::Time::Now());
  if (!maybe_result)
    return {};
  return std::move(maybe_result.value());
}

std::vector<StorageInterestGroup>
InterestGroupStorage::GetInterestGroupsForOwner(const url::Origin& owner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return {};

  absl::optional<std::vector<StorageInterestGroup>> maybe_result =
      DoGetInterestGroupsForOwner(*db_, owner, base::Time::Now());
  if (!maybe_result)
    return {};
  base::UmaHistogramCounts1000("Storage.InterestGroup.PerSiteCount",
                               maybe_result->size());
  return std::move(maybe_result.value());
}

std::vector<StorageInterestGroup>
InterestGroupStorage::GetInterestGroupsForUpdate(const url::Origin& owner,
                                                 size_t groups_limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return {};

  absl::optional<std::vector<StorageInterestGroup>> maybe_result =
      DoGetInterestGroupsForOwner(*db_, owner, base::Time::Now(),
                                  /*get_groups_for_update=*/true);
  if (!maybe_result)
    return {};
  base::RandomShuffle(maybe_result->begin(), maybe_result->end());
  maybe_result->resize(std::min(maybe_result->size(), groups_limit));
  return std::move(maybe_result.value());
}

std::vector<url::Origin>
InterestGroupStorage::GetAllInterestGroupJoiningOrigins() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return {};
  absl::optional<std::vector<url::Origin>> maybe_result =
      DoGetAllInterestGroupJoiningOrigins(*db_, base::Time::Now());
  if (!maybe_result)
    return {};
  return std::move(maybe_result.value());
}

std::vector<std::pair<url::Origin, url::Origin>>
InterestGroupStorage::GetAllInterestGroupOwnerJoinerPairs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return {};
  absl::optional<std::vector<std::pair<url::Origin, url::Origin>>>
      maybe_result =
          DoGetAllInterestGroupOwnerJoinerPairs(*db_, base::Time::Now());
  if (!maybe_result)
    return {};
  return std::move(maybe_result.value());
}

void InterestGroupStorage::RemoveInterestGroupsMatchingOwnerAndJoiner(
    url::Origin owner,
    url::Origin joining_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return;

  if (!DoRemoveInterestGroupsMatchingOwnerAndJoiner(*db_, owner, joining_origin,
                                                    base::Time::Now()))
    DLOG(ERROR)
        << "Could not remove interest groups matching owner and joiner: "
        << db_->GetErrorMessage();

  return;
}

void InterestGroupStorage::DeleteInterestGroupData(
    StoragePartition::StorageKeyMatcherFunction storage_key_matcher) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return;

  if (!DoDeleteInterestGroupData(*db_, storage_key_matcher)) {
    DLOG(ERROR) << "Could not delete interest group data: "
                << db_->GetErrorMessage();
  }
}

void InterestGroupStorage::DeleteAllInterestGroupData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return;

  db_->RazeAndClose();
  db_.reset();
}

void InterestGroupStorage::SetInterestGroupPriority(
    const blink::InterestGroupKey& group_key,
    double priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return;

  if (!DoSetInterestGroupPriority(*db_, group_key, priority)) {
    DLOG(ERROR) << "Could not set interest group priority: "
                << db_->GetErrorMessage();
  }
}

void InterestGroupStorage::UpdateInterestGroupPriorityOverrides(
    const blink::InterestGroupKey& group_key,
    base::flat_map<std::string,
                   auction_worklet::mojom::PrioritySignalsDoublePtr>
        update_priority_signals_overrides) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return;

  blink::InterestGroup group;
  if (!DoLoadInterestGroup(*db_, group_key, group))
    return;

  MergePrioritySignalsOverrides(update_priority_signals_overrides,
                                group.priority_signals_overrides);
  if (!group.IsValid()) {
    // TODO(mmenke): Report errors to devtools.
    return;
  }

  if (!DoSetInterestGroupPrioritySignalsOverrides(
          *db_, group_key, group.priority_signals_overrides)) {
    DLOG(ERROR) << "Could not set interest group priority signals overrides: "
                << db_->GetErrorMessage();
  }
}

void InterestGroupStorage::PerformDBMaintenance() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  last_maintenance_time_ = base::Time::Now();
  ops_since_last_maintenance_ = 0;
  int64_t db_size;
  if (base::GetFileSize(path_to_database_, &db_size)) {
    UMA_HISTOGRAM_MEMORY_KB("Storage.InterestGroup.DBSize", db_size / 1024);
  }
  if (EnsureDBInitialized()) {
    DoPerformDatabaseMaintenance(
        *db_, last_maintenance_time_, /*max_owners=*/max_owners_,
        /*max_owner_interest_groups=*/max_owner_interest_groups_);
  }
}

std::vector<StorageInterestGroup>
InterestGroupStorage::GetAllInterestGroupsUnfilteredForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return {};
  const base::Time distant_past = base::Time::Min();
  std::vector<StorageInterestGroup> result;
  absl::optional<std::vector<url::Origin>> maybe_owners =
      DoGetAllInterestGroupOwners(*db_, distant_past);
  if (!maybe_owners)
    return {};
  for (const auto& owner : *maybe_owners) {
    absl::optional<std::vector<StorageInterestGroup>> maybe_owner_results =
        DoGetInterestGroupsForOwner(*db_, owner, distant_past);
    DCHECK(maybe_owner_results) << owner;
    std::move(maybe_owner_results->begin(), maybe_owner_results->end(),
              std::back_inserter(result));
  }
  return result;
}

base::Time InterestGroupStorage::GetLastMaintenanceTimeForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return last_maintenance_time_;
}

void InterestGroupStorage::DatabaseErrorCallback(int extended_error,
                                                 sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Only save the basic error code (not extended) to UMA.
  base::UmaHistogramExactLinear("Storage.InterestGroup.DBErrors",
                                extended_error & 0xFF,
                                /*sqlite error max+1*/ SQLITE_WARNING + 1);

  if (sql::IsErrorCatastrophic(extended_error)) {
    // Normally this will poison the database, causing any subsequent operations
    // to silently fail without any side effects. However, if RazeAndClose() is
    // called from the error callback in response to an error raised from within
    // sql::Database::Open, opening the now-razed database will be retried.
    db_->RazeAndClose();
    return;
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error))
    DLOG(FATAL) << db_->GetErrorMessage();
}

}  // namespace content
