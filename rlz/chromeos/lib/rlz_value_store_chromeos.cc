// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rlz/chromeos/lib/rlz_value_store_chromeos.h"

#include <algorithm>
#include <string_view>
#include <tuple>

#include "base/base_paths.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/system/factory_ping_embargo_check.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "dbus/bus.h"
#include "rlz/lib/lib_values.h"
#include "rlz/lib/recursive_cross_process_lock_posix.h"
#include "rlz/lib/supplementary_branding.h"
#include "rlz/lib/time_util.h"

namespace rlz_lib {

namespace {

// Key names.
const char kPingTimeKey[] = "ping_time";
const char kAccessPointKey[] = "access_points";
const char kProductEventKey[] = "product_events";
const char kStatefulEventKey[] = "stateful_events";

// Brand name used when there is no supplementary brand name.
const char kNoSupplementaryBrand[] = "_";

// RLZ store filename.
const base::FilePath::CharType kRLZDataFileName[] =
    FILE_PATH_LITERAL("RLZ Data");

// RLZ store lock filename
const base::FilePath::CharType kRLZLockFileName[] =
    FILE_PATH_LITERAL("RLZ Data.lock");

// RLZ store path for testing.
base::LazyInstance<base::FilePath>::Leaky g_testing_rlz_store_path =
    LAZY_INSTANCE_INITIALIZER;

base::FilePath GetRlzStorePathCommon() {
  base::FilePath homedir;
  base::PathService::Get(base::DIR_HOME, &homedir);
  return g_testing_rlz_store_path.Get().empty()
             ? homedir
             : g_testing_rlz_store_path.Get();
}

// Returns file path of the RLZ storage.
base::FilePath GetRlzStorePath() {
  return GetRlzStorePathCommon().Append(kRLZDataFileName);
}

// Returns file path of the RLZ storage lock file.
base::FilePath GetRlzStoreLockPath() {
  return GetRlzStorePathCommon().Append(kRLZLockFileName);
}

// Returns the dictionary key for storing access point-related prefs.
std::string GetKeyName(const std::string& key, AccessPoint access_point) {
  std::string brand = SupplementaryBranding::GetBrand();
  if (brand.empty())
    brand = kNoSupplementaryBrand;
  return key + "." + GetAccessPointName(access_point) + "." + brand;
}

// Returns the dictionary key for storing product-related prefs.
std::string GetKeyName(const std::string& key, Product product) {
  std::string brand = SupplementaryBranding::GetBrand();
  if (brand.empty())
    brand = kNoSupplementaryBrand;
  return key + "." + GetProductName(product) + "." + brand;
}

// Uses |brand| to replace the brand code contained in |rlz|. No-op if |rlz| is
// in incorrect format or already contains |brand|. Returns whether the
// replacement took place.
bool ConvertToDynamicRlz(const std::string& brand,
                         std::string* rlz,
                         AccessPoint access_point) {
  if (brand.size() != 4) {
    LOG(ERROR) << "Invalid brand code format: " + brand;
    return false;
  }
  // Do a sanity check for the rlz string format. It must start with a
  // single-digit rlz encoding version, followed by a two-alphanum access point
  // name, and a four-letter brand code.
  if (rlz->size() < 7 ||
      rlz->substr(1, 2) != GetAccessPointName(access_point)) {
    LOG(ERROR) << "Invalid rlz string format: " + *rlz;
    return false;
  }
  if (rlz->substr(3, 4) == brand)
    return false;
  rlz->replace(3, 4, brand);
  return true;
}

// Forward declare so that it could be referred in SetRlzPingSent.
void OnSetRlzPingSent(int retry_count, bool success);

// Calls debug daemon client to set |should_send_rlz_ping| to 0 in RW_VPD.
// Re-post the work on DBus's original thread if it is not called from there
// because DBus code is not thread safe.
void SetRlzPingSent(int retry_count) {
  // GetSystemBus() could return null in tests.
  base::SequencedTaskRunner* const origin_task_runner =
      ash::DBusThreadManager::Get()->GetSystemBus()
          ? ash::DBusThreadManager::Get()->GetSystemBus()->GetOriginTaskRunner()
          : nullptr;
  if (origin_task_runner && !origin_task_runner->RunsTasksInCurrentSequence()) {
    origin_task_runner->PostTask(FROM_HERE,
                                 base::BindOnce(&SetRlzPingSent, retry_count));
    return;
  }

  ash::DebugDaemonClient::Get()->SetRlzPingSent(
      base::BindOnce(&OnSetRlzPingSent, retry_count + 1));
}

// Callback invoked for DebugDaemonClient::SetRlzPingSent.
void OnSetRlzPingSent(int retry_count, bool success) {
  if (success) {
    return;
  }

  if (retry_count >= RlzValueStoreChromeOS::kMaxRetryCount) {
    LOG(ERROR) << "Setting " << ash::system::kShouldSendRlzPingKey
               << " failed after " << RlzValueStoreChromeOS::kMaxRetryCount
               << " attempts.";
    return;
  }

  SetRlzPingSent(retry_count);
}

// Copy |value| without empty children.
std::optional<base::Value> CopyWithoutEmptyChildren(const base::Value& value) {
  switch (value.type()) {
    case base::Value::Type::DICT: {
      base::Value::Dict dict;
      const base::Value::Dict& dict_in = value.GetDict();

      for (auto it = dict_in.begin(); it != dict_in.end(); ++it) {
        std::optional<base::Value> item_copy =
            CopyWithoutEmptyChildren(it->second);
        if (item_copy)
          dict.Set(it->first, std::move(*item_copy));
      }

      if (dict.empty())
        return std::nullopt;

      return base::Value(std::move(dict));
    }

    case base::Value::Type::LIST: {
      base::Value::List list;
      list.reserve(value.GetList().size());

      for (const base::Value& item : value.GetList()) {
        std::optional<base::Value> item_copy = CopyWithoutEmptyChildren(item);
        if (item_copy)
          list.Append(std::move(*item_copy));
      }

      if (list.empty())
        return std::nullopt;

      return base::Value(std::move(list));
    }

    default:
      return value.Clone();
  }
}

}  // namespace

const int RlzValueStoreChromeOS::kMaxRetryCount = 3;

RlzValueStoreChromeOS::RlzValueStoreChromeOS(const base::FilePath& store_path)
    : store_path_(store_path) {
  ReadStore();
}

RlzValueStoreChromeOS::~RlzValueStoreChromeOS() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  WriteStore();
}

bool RlzValueStoreChromeOS::HasAccess(AccessType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return type == kReadAccess || !read_only_;
}

bool RlzValueStoreChromeOS::WritePingTime(Product product, int64_t time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rlz_store_.SetByDottedPath(GetKeyName(kPingTimeKey, product),
                             base::NumberToString(time));
  return true;
}

bool RlzValueStoreChromeOS::ReadPingTime(Product product, int64_t* time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(wzang): Make sure time is correct (check that npupdate has updated
  // successfully). See AutoEnrollmentController::SystemClockSyncWaiter for
  // potential refactor in the ping embargo class.
  if (!HasRlzEmbargoEndDatePassed()) {
    *time = GetSystemTimeAsInt64();
    return true;
  }

  const std::string* ping_time =
      rlz_store_.FindStringByDottedPath(GetKeyName(kPingTimeKey, product));
  return ping_time ? base::StringToInt64(*ping_time, time) : false;
}

bool RlzValueStoreChromeOS::ClearPingTime(Product product) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rlz_store_.RemoveByDottedPath(GetKeyName(kPingTimeKey, product));
  return true;
}

bool RlzValueStoreChromeOS::WriteAccessPointRlz(AccessPoint access_point,
                                                const char* new_rlz) {
  // If an access point already exists, don't overwrite it.  This is to prevent
  // writing cohort data for first search which is not needed in Chrome OS.
  //
  // There are two possible cases: either the user performs a search before the
  // first ping is sent on first run, or they do not.  If they do, then
  // |new_rlz| contain cohorts for install and first search, but they will be
  // the same.  If they don't, the first time WriteAccessPointRlz() is called
  // |new_rlz| will contain only install cohort.  The second time it will
  // contain both install and first search cohorts.  Ignoring the second
  // means the first search cohort will never be stored.
  if (HasAccessPointRlz(access_point))
    return true;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rlz_store_.SetByDottedPath(GetKeyName(kAccessPointKey, access_point),
                             new_rlz);
  return true;
}

bool RlzValueStoreChromeOS::ReadAccessPointRlz(AccessPoint access_point,
                                               char* rlz,
                                               size_t rlz_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string* rlz_value = rlz_store_.FindStringByDottedPath(
      GetKeyName(kAccessPointKey, access_point));
  if (rlz_value && rlz_value->size() < rlz_size) {
    strncpy(rlz, rlz_value->c_str(), rlz_size);
    return true;
  }
  if (rlz_size > 0)
    *rlz = '\0';
  return rlz_value == nullptr;
}

bool RlzValueStoreChromeOS::ClearAccessPointRlz(AccessPoint access_point) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rlz_store_.RemoveByDottedPath(GetKeyName(kAccessPointKey, access_point));
  return true;
}

bool RlzValueStoreChromeOS::UpdateExistingAccessPointRlz(
    const std::string& brand) {
  DCHECK(SupplementaryBranding::GetBrand().empty());
  bool updated = false;
  for (int i = NO_ACCESS_POINT + 1; i < LAST_ACCESS_POINT; ++i) {
    AccessPoint access_point = static_cast<AccessPoint>(i);
    const std::string access_point_key =
        GetKeyName(kAccessPointKey, access_point);
    const std::string* rlz =
        rlz_store_.FindStringByDottedPath(access_point_key);
    if (rlz) {
      std::string rlz_copy = *rlz;
      if (ConvertToDynamicRlz(brand, &rlz_copy, access_point)) {
        rlz_store_.SetByDottedPath(access_point_key, rlz_copy);
        updated = true;
      }
    }
  }
  return updated;
}

bool RlzValueStoreChromeOS::AddProductEvent(Product product,
                                            const char* event_rlz) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return AddValueToList(GetKeyName(kProductEventKey, product),
                        base::Value(event_rlz));
}

bool RlzValueStoreChromeOS::ReadProductEvents(
    Product product,
    std::vector<std::string>* events) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value::List* events_list =
      rlz_store_.FindListByDottedPath(GetKeyName(kProductEventKey, product));
  if (!events_list)
    return false;

  events->clear();

  bool remove_caf = false;
  for (const base::Value& item : *events_list) {
    const std::string* event = item.GetIfString();
    if (!event)
      continue;

    if (*event == "CAF" && IsStatefulEvent(product, "CAF"))
      remove_caf = true;

    events->push_back(*event);
  }

  if (remove_caf)
    ClearProductEvent(product, "CAF");

  return true;
}

bool RlzValueStoreChromeOS::ClearProductEvent(Product product,
                                              const char* event_rlz) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return RemoveValueFromList(GetKeyName(kProductEventKey, product),
                             base::Value(event_rlz));
}

bool RlzValueStoreChromeOS::ClearAllProductEvents(Product product) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rlz_store_.RemoveByDottedPath(GetKeyName(kProductEventKey, product));
  return true;
}

bool RlzValueStoreChromeOS::AddStatefulEvent(Product product,
                                             const char* event_rlz) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (strcmp(event_rlz, "CAF") == 0)
    SetRlzPingSent(/*retry_count=*/0);

  return AddValueToList(GetKeyName(kStatefulEventKey, product),
                        base::Value(event_rlz));
}

bool RlzValueStoreChromeOS::IsStatefulEvent(Product product,
                                            const char* event_rlz) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const bool event_exists = ListContainsValue(
      GetKeyName(kStatefulEventKey, product), base::Value(event_rlz));

  if (strcmp(event_rlz, "CAF") == 0) {
    ash::system::StatisticsProvider* stats =
        ash::system::StatisticsProvider::GetInstance();
    if (const std::optional<std::string_view> should_send_rlz_ping_value =
            stats->GetMachineStatistic(ash::system::kShouldSendRlzPingKey)) {
      if (should_send_rlz_ping_value ==
          ash::system::kShouldSendRlzPingValueFalse) {
        return true;
      } else if (should_send_rlz_ping_value !=
                 ash::system::kShouldSendRlzPingValueTrue) {
        LOG(WARNING) << ash::system::kShouldSendRlzPingKey
                     << " has an unexpected value: "
                     << should_send_rlz_ping_value.value() << ". Treat it as "
                     << ash::system::kShouldSendRlzPingValueFalse
                     << " to avoid sending duplicate rlz ping.";
        return true;
      }
      if (!HasRlzEmbargoEndDatePassed())
        return true;
    } else {
      // If |kShouldSendRlzPingKey| doesn't exist in RW_VPD, treat it in the
      // same way with the case of |kShouldSendRlzPingValueFalse|.
      return true;
    }
  }

  return event_exists;
}

bool RlzValueStoreChromeOS::ClearAllStatefulEvents(Product product) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rlz_store_.RemoveByDottedPath(GetKeyName(kStatefulEventKey, product));
  return true;
}

void RlzValueStoreChromeOS::CollectGarbage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

// static
bool RlzValueStoreChromeOS::HasRlzEmbargoEndDatePassed() {
  ash::system::StatisticsProvider* statistics_provider =
      ash::system::StatisticsProvider::GetInstance();
  return ash::system::GetRlzPingEmbargoState(statistics_provider) !=
         ash::system::FactoryPingEmbargoState::kNotPassed;
}

void RlzValueStoreChromeOS::ReadStore() {
  int error_code = 0;
  std::string error_msg;
  JSONFileValueDeserializer deserializer(store_path_);
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_msg);
  switch (error_code) {
    case JSONFileValueDeserializer::JSON_NO_SUCH_FILE:
      read_only_ = false;
      break;
    case JSONFileValueDeserializer::JSON_NO_ERROR:
      read_only_ = false;
      if (value->is_dict()) {
        rlz_store_ = std::move(value->GetDict());
      } else {
        LOG(ERROR) << "RLZ store is not a dict";
        rlz_store_.clear();
      }
      break;
    default:
      LOG(ERROR) << "Error reading RLZ store: " << error_msg;
  }
}

void RlzValueStoreChromeOS::WriteStore() {
  std::string json_data;
  JSONStringValueSerializer serializer(&json_data);
  serializer.set_pretty_print(true);

  base::Value copy = CopyWithoutEmptyChildren(base::Value(rlz_store_.Clone()))
                         .value_or(base::Value(base::Value::Type::DICT));
  if (!serializer.Serialize(copy)) {
    NOTREACHED() << "Failed to serialize RLZ data";
  }
  if (!base::ImportantFileWriter::WriteFileAtomically(store_path_, json_data))
    LOG(ERROR) << "Error writing RLZ store";
}

bool RlzValueStoreChromeOS::AddValueToList(const std::string& list_name,
                                           base::Value value) {
  base::Value::List* list = rlz_store_.FindListByDottedPath(list_name);
  if (!list) {
    list =
        &rlz_store_
             .SetByDottedPath(list_name, base::Value(base::Value::Type::LIST))
             ->GetList();
  }
  if (!base::Contains(*list, value)) {
    list->Append(std::move(value));
  }
  return true;
}

bool RlzValueStoreChromeOS::RemoveValueFromList(const std::string& list_name,
                                                const base::Value& to_remove) {
  base::Value::List* list = rlz_store_.FindListByDottedPath(list_name);
  if (!list)
    return false;

  list->EraseIf(
      [&to_remove](const base::Value& value) { return value == to_remove; });

  return true;
}

bool RlzValueStoreChromeOS::ListContainsValue(const std::string& list_name,
                                              const base::Value& value) const {
  const base::Value::List* list = rlz_store_.FindListByDottedPath(list_name);
  if (!list)
    return false;

  return base::Contains(*list, value);
}

bool RlzValueStoreChromeOS::HasAccessPointRlz(AccessPoint access_point) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string* value = rlz_store_.FindStringByDottedPath(
      GetKeyName(kAccessPointKey, access_point));
  return value && !value->empty();
}

namespace {

// RlzValueStoreChromeOS keeps its data in memory and only writes it to disk
// when ScopedRlzValueStoreLock goes out of scope. Hence, if several
// ScopedRlzValueStoreLocks are nested, they all need to use the same store
// object.

RecursiveCrossProcessLock g_recursive_lock =
    RECURSIVE_CROSS_PROCESS_LOCK_INITIALIZER;

// This counts the nesting depth of |ScopedRlzValueStoreLock|.
int g_lock_depth = 0;

// This is the shared store object. Non-|NULL| only when |g_lock_depth > 0|.
RlzValueStoreChromeOS* g_store = nullptr;

}  // namespace

ScopedRlzValueStoreLock::ScopedRlzValueStoreLock() {
  bool got_cross_process_lock =
      g_recursive_lock.TryGetCrossProcessLock(GetRlzStoreLockPath());
  // At this point, we hold the in-process lock, no matter the value of
  // |got_cross_process_lock|.

  ++g_lock_depth;
  if (!got_cross_process_lock) {
    // Acquiring cross-process lock failed, so simply return here.
    // In-process lock will be released in dtor.
    DCHECK(!g_store);
    return;
  }

  if (g_lock_depth > 1) {
    // Reuse the already existing store object.
    DCHECK(g_store);
    store_.reset(g_store);
    return;
  }

  // This is the topmost lock, create a new store object.
  DCHECK(!g_store);
  g_store = new RlzValueStoreChromeOS(GetRlzStorePath());
  store_.reset(g_store);
}

ScopedRlzValueStoreLock::~ScopedRlzValueStoreLock() {
  --g_lock_depth;
  DCHECK_GE(g_lock_depth, 0);

  if (g_lock_depth > 0) {
    // Other locks are still using store_, so don't free it yet.
    std::ignore = store_.release();
    return;
  }

  g_store = nullptr;

  g_recursive_lock.ReleaseLock();
}

RlzValueStore* ScopedRlzValueStoreLock::GetStore() {
  return store_.get();
}

namespace testing {

void SetRlzStoreDirectory(const base::FilePath& directory) {
  g_testing_rlz_store_path.Get() = directory;
}

std::string RlzStoreFilenameStr() {
  return GetRlzStorePath().value();
}

}  // namespace testing

}  // namespace rlz_lib
