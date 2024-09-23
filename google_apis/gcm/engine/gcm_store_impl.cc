// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/gcm_store_impl.h"

#include <string_view>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/time/time.h"
#include "google_apis/gcm/base/encryptor.h"
#include "google_apis/gcm/base/gcm_constants.h"
#include "google_apis/gcm/base/gcm_features.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace gcm {

namespace {

// This enum is used in an UMA histogram (GCMLoadStatus enum defined in
// tools/metrics/histograms/histogram.xml). Hence the entries here shouldn't
// be deleted or re-ordered and new ones should be added to the end.
enum LoadStatus {
  LOADING_SUCCEEDED,
  RELOADING_OPEN_STORE,
  OPENING_STORE_FAILED,
  LOADING_DEVICE_CREDENTIALS_FAILED,
  LOADING_REGISTRATION_FAILED,
  LOADING_INCOMING_MESSAGES_FAILED,
  LOADING_OUTGOING_MESSAGES_FAILED,
  LOADING_LAST_CHECKIN_INFO_FAILED,
  LOADING_GSERVICE_SETTINGS_FAILED,
  LOADING_ACCOUNT_MAPPING_FAILED,
  LOADING_LAST_TOKEN_TIME_FAILED,
  LOADING_HEARTBEAT_INTERVALS_FAILED,
  LOADING_INSTANCE_ID_DATA_FAILED,
  STORE_DOES_NOT_EXIST,

  // NOTE: always keep this entry at the end. Add new status types only
  // immediately above this line. Make sure to update the corresponding
  // histogram enum accordingly.
  LOAD_STATUS_COUNT
};

// Limit to the number of outstanding messages per app.
const int kMessagesPerAppLimit = 20;

// Separator used to split persistent ID and expiration time.
constexpr char kIncomingMsgSeparator[] = "|";

// ---- LevelDB keys. ----
// Key for this device's android id.
const char kDeviceAIDKey[] = "device_aid_key";
// Key for this device's android security token.
const char kDeviceTokenKey[] = "device_token_key";
// Lowest lexicographically ordered app ids.
// Used for prefixing app id.
const char kRegistrationKeyStart[] = "reg1-";
// Key guaranteed to be higher than all app ids.
// Used for limiting iteration.
const char kRegistrationKeyEnd[] = "reg2-";
// Lowest lexicographically ordered incoming message key.
// Used for prefixing messages.
const char kIncomingMsgKeyStart[] = "incoming1-";
// Key guaranteed to be higher than all incoming message keys.
// Used for limiting iteration.
const char kIncomingMsgKeyEnd[] = "incoming2-";
// Lowest lexicographically ordered outgoing message key.
// Used for prefixing outgoing messages.
const char kOutgoingMsgKeyStart[] = "outgoing1-";
// Key guaranteed to be higher than all outgoing message keys.
// Used for limiting iteration.
const char kOutgoingMsgKeyEnd[] = "outgoing2-";
// Lowest lexicographically ordered G-service settings key.
// Used for prefixing G-services settings.
const char kGServiceSettingKeyStart[] = "gservice1-";
// Key guaranteed to be higher than all G-services settings keys.
// Used for limiting iteration.
const char kGServiceSettingKeyEnd[] = "gservice2-";
// Key for digest of the last G-services settings update.
const char kGServiceSettingsDigestKey[] = "gservices_digest";
// Key used to indicate how many accounts were last checked in with this device.
const char kLastCheckinAccountsKey[] = "last_checkin_accounts_count";
// Key used to timestamp last checkin (marked with G services settings update).
const char kLastCheckinTimeKey[] = "last_checkin_time";
// Lowest lexicographically ordered account key.
// Used for prefixing account information.
const char kAccountKeyStart[] = "account1-";
// Key guaranteed to be higher than all account keys.
// Used for limiting iteration.
const char kAccountKeyEnd[] = "account2-";
// Lowest lexicographically ordered heartbeat key.
// Used for prefixing account information.
const char kHeartbeatKeyStart[] = "heartbeat1-";
// Key guaranteed to be higher than all heartbeat keys.
// Used for limiting iteration.
const char kHeartbeatKeyEnd[] = "heartbeat2-";
// Key used for last token fetch time.
const char kLastTokenFetchTimeKey[] = "last_token_fetch_time";
// Lowest lexicographically ordered app ids.
// Used for prefixing app id.
const char kInstanceIDKeyStart[] = "iid1-";
// Key guaranteed to be higher than all app ids.
// Used for limiting iteration.
const char kInstanceIDKeyEnd[] = "iid2-";

std::string MakeRegistrationKey(const std::string& app_id) {
  return kRegistrationKeyStart + app_id;
}

std::string ParseRegistrationKey(const std::string& key) {
  return key.substr(std::size(kRegistrationKeyStart) - 1);
}

std::string MakeIncomingKey(const std::string& persistent_id) {
  return kIncomingMsgKeyStart + persistent_id;
}

std::string MakeIncomingData(const std::string& persistent_id) {
  return base::StrCat(
      {persistent_id, kIncomingMsgSeparator,
       base::NumberToString(
           base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds() +
           kIncomingMessageTTL.InMicroseconds())});
}

std::string MakeOutgoingKey(const std::string& persistent_id) {
  return kOutgoingMsgKeyStart + persistent_id;
}

std::string ParseOutgoingKey(const std::string& key) {
  return key.substr(std::size(kOutgoingMsgKeyStart) - 1);
}

std::string MakeGServiceSettingKey(const std::string& setting_name) {
  return kGServiceSettingKeyStart + setting_name;
}

std::string ParseGServiceSettingKey(const std::string& key) {
  return key.substr(std::size(kGServiceSettingKeyStart) - 1);
}

std::string MakeAccountKey(const CoreAccountId& account_id) {
  return kAccountKeyStart + account_id.ToString();
}

CoreAccountId ParseAccountKey(const std::string& key) {
  return CoreAccountId::FromString(key.substr(std::size(kAccountKeyStart) - 1));
}

std::string MakeHeartbeatKey(const std::string& scope) {
  return kHeartbeatKeyStart + scope;
}

std::string ParseHeartbeatKey(const std::string& key) {
  return key.substr(std::size(kHeartbeatKeyStart) - 1);
}

std::string MakeInstanceIDKey(const std::string& app_id) {
  return kInstanceIDKeyStart + app_id;
}

std::string ParseInstanceIDKey(const std::string& key) {
  return key.substr(std::size(kInstanceIDKeyStart) - 1);
}

// Note: leveldb::Slice keeps a pointer to the data in |s|, which must therefore
// outlive the slice.
// For example: MakeSlice(MakeOutgoingKey(x)) is invalid.
leveldb::Slice MakeSlice(std::string_view s) {
  return leveldb::Slice(s.data(), s.size());
}

}  // namespace

class GCMStoreImpl::Backend
    : public base::RefCountedThreadSafe<GCMStoreImpl::Backend> {
 public:
  Backend(const base::FilePath& path,
          scoped_refptr<base::SequencedTaskRunner> foreground_runner,
          std::unique_ptr<Encryptor> encryptor);

  // Blocking implementations of GCMStoreImpl methods.
  void Load(StoreOpenMode open_mode, LoadCallback callback);
  void Close();
  void Destroy(UpdateCallback callback);
  void SetDeviceCredentials(uint64_t device_android_id,
                            uint64_t device_security_token,
                            UpdateCallback callback);
  void AddRegistration(const std::string& serialized_key,
                       const std::string& serialized_value,
                       UpdateCallback callback);
  void RemoveRegistration(const std::string& serialized_key,
                          UpdateCallback callback);
  void AddIncomingMessage(const std::string& persistent_id,
                          UpdateCallback callback);
  void RemoveIncomingMessages(const PersistentIdList& persistent_ids,
                              UpdateCallback callback);
  void AddOutgoingMessage(const std::string& persistent_id,
                          const MCSMessage& message,
                          UpdateCallback callback);
  void RemoveOutgoingMessages(
      const PersistentIdList& persistent_ids,
      base::OnceCallback<void(bool, const AppIdToMessageCountMap&)> callback);
  void AddUserSerialNumber(const std::string& username,
                           int64_t serial_number,
                           UpdateCallback callback);
  void RemoveUserSerialNumber(const std::string& username,
                              UpdateCallback callback);
  void SetLastCheckinInfo(const base::Time& time,
                          const std::set<std::string>& accounts,
                          UpdateCallback callback);
  void SetGServicesSettings(const std::map<std::string, std::string>& settings,
                            const std::string& digest,
                            UpdateCallback callback);
  void AddAccountMapping(const AccountMapping& account_mapping,
                         UpdateCallback callback);
  void RemoveAccountMapping(const CoreAccountId& account_id,
                            UpdateCallback callback);
  void SetLastTokenFetchTime(const base::Time& time, UpdateCallback callback);
  void AddHeartbeatInterval(const std::string& scope,
                            int interval_ms,
                            UpdateCallback callback);
  void RemoveHeartbeatInterval(const std::string& scope,
                               UpdateCallback callback);
  void AddInstanceIDData(const std::string& app_id,
                         const std::string& instance_id_data,
                         UpdateCallback callback);
  void RemoveInstanceIDData(const std::string& app_id, UpdateCallback callback);
  void SetValue(const std::string& key,
                const std::string& value,
                UpdateCallback callback);

 private:
  friend class base::RefCountedThreadSafe<Backend>;
  ~Backend();

  LoadStatus OpenStoreAndLoadData(StoreOpenMode open_mode, LoadResult* result);
  bool LoadDeviceCredentials(uint64_t* android_id, uint64_t* security_token);
  bool LoadRegistrations(std::map<std::string, std::string>* registrations);
  bool LoadIncomingMessages(std::vector<std::string>* incoming_messages);
  bool LoadOutgoingMessages(OutgoingMessageMap* outgoing_messages);
  bool LoadLastCheckinInfo(base::Time* last_checkin_time,
                           std::set<std::string>* accounts);
  bool LoadGServicesSettings(std::map<std::string, std::string>* settings,
                             std::string* digest);
  bool LoadAccountMappingInfo(AccountMappings* account_mappings);
  bool LoadLastTokenFetchTime(base::Time* last_token_fetch_time);
  bool LoadHeartbeatIntervals(std::map<std::string, int>* heartbeat_intervals);
  bool LoadInstanceIDData(std::map<std::string, std::string>* instance_id_data);

  const base::FilePath path_;
  scoped_refptr<base::SequencedTaskRunner> foreground_task_runner_;
  std::unique_ptr<Encryptor> encryptor_;

  std::unique_ptr<leveldb::DB> db_;
};

GCMStoreImpl::Backend::Backend(
    const base::FilePath& path,
    scoped_refptr<base::SequencedTaskRunner> foreground_task_runner,
    std::unique_ptr<Encryptor> encryptor)
    : path_(path),
      foreground_task_runner_(foreground_task_runner),
      encryptor_(std::move(encryptor)) {}

GCMStoreImpl::Backend::~Backend() {}

LoadStatus GCMStoreImpl::Backend::OpenStoreAndLoadData(StoreOpenMode open_mode,
                                                       LoadResult* result) {
  LoadStatus load_status;
  if (db_.get()) {
    LOG(ERROR) << "Attempting to reload open database.";
    return RELOADING_OPEN_STORE;
  }

  // Checks if the store exists or not. Opening a db with create_if_missing
  // not set will still create a new directory if the store does not exist.
  if (open_mode == DO_NOT_CREATE &&
      !leveldb_chrome::PossiblyValidDB(path_, leveldb::Env::Default())) {
    DVLOG(2) << "Database " << path_.value() << " does not exist";
    return STORE_DOES_NOT_EXIST;
  }

  leveldb_env::Options options;
  options.create_if_missing = open_mode == CREATE_IF_MISSING;
  options.paranoid_checks = true;

  // GCMStore does not typically handle large amounts of data, nor a
  // high rate of store operations, so limit the write log size to something
  // more appropriate (impact both in-memory and on-disk size before
  // LevelDB compaction will be triggered).
  options.write_buffer_size = 128 * 1024;

  leveldb::Status status =
      leveldb_env::OpenDB(options, path_.AsUTF8Unsafe(), &db_);
  UMA_HISTOGRAM_ENUMERATION("GCM.Database.Open",
                            leveldb_env::GetLevelDBStatusUMAValue(status),
                            leveldb_env::LEVELDB_STATUS_MAX);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to open database " << path_.value() << ": "
               << status.ToString();
    return OPENING_STORE_FAILED;
  }

  if (!LoadDeviceCredentials(&result->device_android_id,
                             &result->device_security_token)) {
    return LOADING_DEVICE_CREDENTIALS_FAILED;
  }
  if (!LoadRegistrations(&result->registrations))
    return LOADING_REGISTRATION_FAILED;
  if (!LoadIncomingMessages(&result->incoming_messages))
    return LOADING_INCOMING_MESSAGES_FAILED;
  if (!LoadOutgoingMessages(&result->outgoing_messages))
    return LOADING_OUTGOING_MESSAGES_FAILED;
  if (!LoadLastCheckinInfo(&result->last_checkin_time,
                           &result->last_checkin_accounts)) {
    return LOADING_LAST_CHECKIN_INFO_FAILED;
  }
  if (!LoadGServicesSettings(&result->gservices_settings,
                             &result->gservices_digest)) {
    return load_status = LOADING_GSERVICE_SETTINGS_FAILED;
  }
  if (!LoadAccountMappingInfo(&result->account_mappings))
    return LOADING_ACCOUNT_MAPPING_FAILED;
  if (!LoadLastTokenFetchTime(&result->last_token_fetch_time))
    return LOADING_LAST_TOKEN_TIME_FAILED;
  if (!LoadHeartbeatIntervals(&result->heartbeat_intervals))
    return LOADING_HEARTBEAT_INTERVALS_FAILED;
  if (!LoadInstanceIDData(&result->instance_id_data))
    return LOADING_INSTANCE_ID_DATA_FAILED;

  return LOADING_SUCCEEDED;
}

void GCMStoreImpl::Backend::Load(StoreOpenMode open_mode,
                                 LoadCallback callback) {
  std::unique_ptr<LoadResult> result(new LoadResult());
  LoadStatus load_status = OpenStoreAndLoadData(open_mode, result.get());

  if (load_status != LOADING_SUCCEEDED) {
    result->Reset();
    result->store_does_not_exist = (load_status == STORE_DOES_NOT_EXIST);
    foreground_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
    return;
  }

  // |result->registrations| contains both GCM registrations and InstanceID
  // tokens. Count them separately.
  int gcm_registration_count = 0;
  int instance_id_token_count = 0;
  for (const auto& registration : result->registrations) {
    if (base::StartsWith(registration.first, "iid-",
                         base::CompareCase::SENSITIVE))
      instance_id_token_count++;
    else
      gcm_registration_count++;
  }

  // Only record histograms if GCM had already been set up for this device.
  if (result->device_android_id != 0 && result->device_security_token != 0) {
    int64_t file_size = 0;
    if (base::GetFileSize(path_, &file_size)) {
      UMA_HISTOGRAM_COUNTS_1M("GCM.StoreSizeKB",
                              static_cast<int>(file_size / 1024));
    }
  }

  DVLOG(1) << "Succeeded in loading "
           << gcm_registration_count << " GCM registrations, "
           << result->incoming_messages.size()
           << " unacknowledged incoming messages "
           << result->outgoing_messages.size()
           << " unacknowledged outgoing messages, "
           << result->instance_id_data.size() << " Instance IDs, "
           << instance_id_token_count << " InstanceID tokens.";
  result->success = true;
  foreground_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
  return;
}

void GCMStoreImpl::Backend::Close() {
  DVLOG(1) << "Closing GCM store.";
  db_.reset();
}

void GCMStoreImpl::Backend::Destroy(UpdateCallback callback) {
  DVLOG(1) << "Destroying GCM store.";
  db_.reset();
  const leveldb::Status s =
      leveldb::DestroyDB(path_.AsUTF8Unsafe(), leveldb_env::Options());
  if (s.ok()) {
    foreground_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    return;
  }
  LOG(ERROR) << "Destroy failed: " << s.ToString();
  foreground_task_runner_->PostTask(FROM_HERE,
                                    base::BindOnce(std::move(callback), false));
}

void GCMStoreImpl::Backend::SetDeviceCredentials(uint64_t device_android_id,
                                                 uint64_t device_security_token,
                                                 UpdateCallback callback) {
  DVLOG(1) << "Saving device credentials with AID " << device_android_id;
  if (!db_.get()) {
    LOG(ERROR) << "GCMStore db doesn't exist.";
    foreground_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  leveldb::WriteOptions write_options;
  write_options.sync = true;

  std::string encrypted_token;
  encryptor_->EncryptString(base::NumberToString(device_security_token),
                            &encrypted_token);
  std::string android_id_str = base::NumberToString(device_android_id);
  leveldb::Status s =
      db_->Put(write_options,
               MakeSlice(kDeviceAIDKey),
               MakeSlice(android_id_str));
  if (s.ok()) {
    s = db_->Put(
        write_options, MakeSlice(kDeviceTokenKey), MakeSlice(encrypted_token));
  }
  if (s.ok()) {
    foreground_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    return;
  }
  LOG(ERROR) << "LevelDB put failed: " << s.ToString();
  foreground_task_runner_->PostTask(FROM_HERE,
                                    base::BindOnce(std::move(callback), false));
}

void GCMStoreImpl::Backend::AddRegistration(const std::string& serialized_key,
                                            const std::string& serialized_value,
                                            UpdateCallback callback) {
  DVLOG(1) << "Saving registration info for app: " << serialized_key;
  if (!db_.get()) {
    LOG(ERROR) << "GCMStore db doesn't exist.";
    foreground_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  leveldb::WriteOptions write_options;
  write_options.sync = true;

  const leveldb::Status status = db_->Put(
      write_options,
      MakeSlice(MakeRegistrationKey(serialized_key)),
      MakeSlice(serialized_value));
  if (!status.ok())
    LOG(ERROR) << "LevelDB put failed: " << status.ToString();
  foreground_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), status.ok()));
}

void GCMStoreImpl::Backend::RemoveRegistration(
    const std::string& serialized_key,
    UpdateCallback callback) {
  if (!db_.get()) {
    LOG(ERROR) << "GCMStore db doesn't exist.";
    foreground_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  leveldb::WriteOptions write_options;
  write_options.sync = true;

  leveldb::Status status = db_->Delete(
      write_options, MakeSlice(MakeRegistrationKey(serialized_key)));
  if (!status.ok())
    LOG(ERROR) << "LevelDB remove failed: " << status.ToString();
  foreground_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), status.ok()));
}

void GCMStoreImpl::Backend::AddIncomingMessage(const std::string& persistent_id,
                                               UpdateCallback callback) {
  DVLOG(1) << "Saving incoming message with id " << persistent_id;
  if (!db_.get()) {
    LOG(ERROR) << "GCMStore db doesn't exist.";
    foreground_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  leveldb::WriteOptions write_options;
  write_options.sync = true;

  std::string key = MakeIncomingKey(persistent_id);
  std::string data = MakeIncomingData(persistent_id);
  const leveldb::Status s =
      db_->Put(write_options, MakeSlice(key), MakeSlice(data));
  if (s.ok()) {
    foreground_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    return;
  }
  LOG(ERROR) << "LevelDB put failed: " << s.ToString();
  foreground_task_runner_->PostTask(FROM_HERE,
                                    base::BindOnce(std::move(callback), false));
}

void GCMStoreImpl::Backend::RemoveIncomingMessages(
    const PersistentIdList& persistent_ids,
    UpdateCallback callback) {
  if (!db_.get()) {
    LOG(ERROR) << "GCMStore db doesn't exist.";
    foreground_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  leveldb::WriteOptions write_options;
  write_options.sync = true;

  leveldb::Status s;
  for (PersistentIdList::const_iterator iter = persistent_ids.begin();
       iter != persistent_ids.end();
       ++iter) {
    DVLOG(1) << "Removing incoming message with id " << *iter;
    std::string key = MakeIncomingKey(*iter);
    s = db_->Delete(write_options, MakeSlice(key));
    if (!s.ok())
      break;
  }
  if (s.ok()) {
    foreground_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    return;
  }
  LOG(ERROR) << "LevelDB remove failed: " << s.ToString();
  foreground_task_runner_->PostTask(FROM_HERE,
                                    base::BindOnce(std::move(callback), false));
}

void GCMStoreImpl::Backend::AddOutgoingMessage(const std::string& persistent_id,
                                               const MCSMessage& message,
                                               UpdateCallback callback) {
  DVLOG(1) << "Saving outgoing message with id " << persistent_id;
  if (!db_.get()) {
    LOG(ERROR) << "GCMStore db doesn't exist.";
    foreground_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  leveldb::WriteOptions write_options;
  write_options.sync = true;

  std::string data =
      static_cast<char>(message.tag()) + message.SerializeAsString();
  std::string key = MakeOutgoingKey(persistent_id);
  const leveldb::Status s = db_->Put(write_options,
                                     MakeSlice(key),
                                     MakeSlice(data));
  if (s.ok()) {
    foreground_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    return;
  }
  LOG(ERROR) << "LevelDB put failed: " << s.ToString();
  foreground_task_runner_->PostTask(FROM_HERE,
                                    base::BindOnce(std::move(callback), false));
}

void GCMStoreImpl::Backend::RemoveOutgoingMessages(
    const PersistentIdList& persistent_ids,
    base::OnceCallback<void(bool, const AppIdToMessageCountMap&)> callback) {
  if (!db_.get()) {
    LOG(ERROR) << "GCMStore db doesn't exist.";
    foreground_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false, AppIdToMessageCountMap()));
    return;
  }
  leveldb::ReadOptions read_options;
  leveldb::WriteOptions write_options;
  write_options.sync = true;

  AppIdToMessageCountMap removed_message_counts;

  leveldb::Status s;
  for (PersistentIdList::const_iterator iter = persistent_ids.begin();
       iter != persistent_ids.end();
       ++iter) {
    DVLOG(1) << "Removing outgoing message with id " << *iter;
    std::string outgoing_message;
    std::string key = MakeOutgoingKey(*iter);
    s = db_->Get(read_options,
                 MakeSlice(key),
                 &outgoing_message);
    if (!s.ok())
      break;
    mcs_proto::DataMessageStanza data_message;
    // Skip the initial tag byte and parse the rest to extract the message.
    if (data_message.ParseFromString(outgoing_message.substr(1))) {
      DCHECK(!data_message.category().empty());
      if (removed_message_counts.count(data_message.category()) != 0)
        removed_message_counts[data_message.category()]++;
      else
        removed_message_counts[data_message.category()] = 1;
    }
    DVLOG(1) << "Removing outgoing message with id " << *iter;
    s = db_->Delete(write_options, MakeSlice(key));
    if (!s.ok())
      break;
  }
  if (s.ok()) {
    foreground_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), true, removed_message_counts));
    return;
  }
  LOG(ERROR) << "LevelDB remove failed: " << s.ToString();
  foreground_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), false, AppIdToMessageCountMap()));
}

void GCMStoreImpl::Backend::SetLastCheckinInfo(
    const base::Time& time,
    const std::set<std::string>& accounts,
    UpdateCallback callback) {
  leveldb::WriteBatch write_batch;

  int64_t last_checkin_time_internal = time.ToInternalValue();
  write_batch.Put(MakeSlice(kLastCheckinTimeKey),
                  MakeSlice(base::NumberToString(last_checkin_time_internal)));

  std::string serialized_accounts;
  for (std::set<std::string>::iterator iter = accounts.begin();
       iter != accounts.end();
       ++iter) {
    serialized_accounts += *iter;
    serialized_accounts += ",";
  }
  if (!serialized_accounts.empty())
    serialized_accounts.erase(serialized_accounts.length() - 1);

  write_batch.Put(MakeSlice(kLastCheckinAccountsKey),
                  MakeSlice(serialized_accounts));

  leveldb::WriteOptions write_options;
  write_options.sync = true;
  const leveldb::Status s = db_->Write(write_options, &write_batch);

  if (!s.ok())
    LOG(ERROR) << "LevelDB set last checkin info failed: " << s.ToString();
  foreground_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), s.ok()));
}

void GCMStoreImpl::AddInstanceIDData(const std::string& app_id,
                                     const std::string& instance_id_data,
                                     UpdateCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMStoreImpl::Backend::AddInstanceIDData, backend_,
                     app_id, instance_id_data, std::move(callback)));
}

void GCMStoreImpl::RemoveInstanceIDData(const std::string& app_id,
                                        UpdateCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GCMStoreImpl::Backend::RemoveInstanceIDData,
                                backend_, app_id, std::move(callback)));
}

void GCMStoreImpl::Backend::SetGServicesSettings(
    const std::map<std::string, std::string>& settings,
    const std::string& settings_digest,
    UpdateCallback callback) {
  leveldb::WriteBatch write_batch;

  // Remove all existing settings.
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;
  std::unique_ptr<leveldb::Iterator> db_it(db_->NewIterator(read_options));
  for (db_it->Seek(MakeSlice(kGServiceSettingKeyStart));
       db_it->Valid() && db_it->key().ToString() < kGServiceSettingKeyEnd;
       db_it->Next()) {
    write_batch.Delete(db_it->key());
  }

  // Add the new settings.
  for (std::map<std::string, std::string>::const_iterator map_it =
           settings.begin();
       map_it != settings.end(); ++map_it) {
    write_batch.Put(MakeSlice(MakeGServiceSettingKey(map_it->first)),
                    MakeSlice(map_it->second));
  }

  // Update the settings digest.
  write_batch.Put(MakeSlice(kGServiceSettingsDigestKey),
                  MakeSlice(settings_digest));

  // Write it all in a batch.
  leveldb::WriteOptions write_options;
  write_options.sync = true;

  leveldb::Status s = db_->Write(write_options, &write_batch);
  if (!s.ok())
    LOG(ERROR) << "LevelDB GService Settings update failed: " << s.ToString();
  foreground_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), s.ok()));
}

void GCMStoreImpl::Backend::AddAccountMapping(
    const AccountMapping& account_mapping,
    UpdateCallback callback) {
  DVLOG(1) << "Saving account info for account with email: "
           << account_mapping.email;
  if (!db_.get()) {
    LOG(ERROR) << "GCMStore db doesn't exist.";
    foreground_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  leveldb::WriteOptions write_options;
  write_options.sync = true;

  std::string data = account_mapping.SerializeAsString();
  std::string key = MakeAccountKey(account_mapping.account_id);
  const leveldb::Status s =
      db_->Put(write_options, MakeSlice(key), MakeSlice(data));
  if (!s.ok())
    LOG(ERROR) << "LevelDB adding account mapping failed: " << s.ToString();
  foreground_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), s.ok()));
}

void GCMStoreImpl::Backend::RemoveAccountMapping(
    const CoreAccountId& account_id,
    UpdateCallback callback) {
  if (!db_.get()) {
    LOG(ERROR) << "GCMStore db doesn't exist.";
    foreground_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  leveldb::WriteOptions write_options;
  write_options.sync = true;

  leveldb::Status s =
      db_->Delete(write_options, MakeSlice(MakeAccountKey(account_id)));

  if (!s.ok())
    LOG(ERROR) << "LevelDB removal of account mapping failed: " << s.ToString();
  foreground_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), s.ok()));
}

void GCMStoreImpl::Backend::SetLastTokenFetchTime(const base::Time& time,
                                                  UpdateCallback callback) {
  DVLOG(1) << "Setting last token fetching time.";
  if (!db_.get()) {
    LOG(ERROR) << "GCMStore db doesn't exist.";
    foreground_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  leveldb::WriteOptions write_options;
  write_options.sync = true;

  const leveldb::Status s =
      db_->Put(write_options, MakeSlice(kLastTokenFetchTimeKey),
               MakeSlice(base::NumberToString(time.ToInternalValue())));

  if (!s.ok())
    LOG(ERROR) << "LevelDB setting last token fetching time: " << s.ToString();
  foreground_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), s.ok()));
}

void GCMStoreImpl::Backend::AddHeartbeatInterval(const std::string& scope,
                                                 int interval_ms,
                                                 UpdateCallback callback) {
  DVLOG(1) << "Saving a heartbeat interval: scope: " << scope
           << " interval: " << interval_ms << "ms.";
  if (!db_.get()) {
    LOG(ERROR) << "GCMStore db doesn't exist.";
    foreground_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  leveldb::WriteOptions write_options;
  write_options.sync = true;

  std::string data = base::NumberToString(interval_ms);
  std::string key = MakeHeartbeatKey(scope);
  const leveldb::Status s =
      db_->Put(write_options, MakeSlice(key), MakeSlice(data));
  if (!s.ok())
    LOG(ERROR) << "LevelDB adding heartbeat interval failed: " << s.ToString();
  foreground_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), s.ok()));
}

void GCMStoreImpl::Backend::RemoveHeartbeatInterval(const std::string& scope,
                                                    UpdateCallback callback) {
  if (!db_.get()) {
    LOG(ERROR) << "GCMStore db doesn't exist.";
    foreground_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  leveldb::WriteOptions write_options;
  write_options.sync = true;

  leveldb::Status s =
      db_->Delete(write_options, MakeSlice(MakeHeartbeatKey(scope)));

  if (!s.ok()) {
    LOG(ERROR) << "LevelDB removal of heartbeat interval failed: "
               << s.ToString();
  }
  foreground_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), s.ok()));
}

void GCMStoreImpl::Backend::AddInstanceIDData(
    const std::string& app_id,
    const std::string& instance_id_data,
    UpdateCallback callback) {
  DVLOG(1) << "Adding Instance ID data.";
  if (!db_.get()) {
    LOG(ERROR) << "GCMStore db doesn't exist.";
    foreground_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  leveldb::WriteOptions write_options;
  write_options.sync = true;

  std::string key = MakeInstanceIDKey(app_id);
  const leveldb::Status status = db_->Put(write_options,
                                          MakeSlice(key),
                                          MakeSlice(instance_id_data));
  if (!status.ok())
    LOG(ERROR) << "LevelDB put failed: " << status.ToString();
  foreground_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), status.ok()));
}

void GCMStoreImpl::Backend::RemoveInstanceIDData(const std::string& app_id,
                                                 UpdateCallback callback) {
  if (!db_.get()) {
    LOG(ERROR) << "GCMStore db doesn't exist.";
    foreground_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  leveldb::WriteOptions write_options;
  write_options.sync = true;

  leveldb::Status status =
      db_->Delete(write_options, MakeSlice(MakeInstanceIDKey(app_id)));
  if (!status.ok())
    LOG(ERROR) << "LevelDB remove failed: " << status.ToString();
  foreground_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), status.ok()));
}

void GCMStoreImpl::Backend::SetValue(const std::string& key,
                                     const std::string& value,
                                     UpdateCallback callback) {
  DVLOG(1) << "Injecting a value to GCM Store for testing. Key: "
           << key << ", Value: " << value;
  if (!db_.get()) {
    LOG(ERROR) << "GCMStore db doesn't exist.";
    foreground_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  leveldb::WriteOptions write_options;
  write_options.sync = true;

  const leveldb::Status s =
      db_->Put(write_options, MakeSlice(key), MakeSlice(value));

  if (!s.ok())
    LOG(ERROR) << "LevelDB had problems injecting a value: " << s.ToString();
  foreground_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), s.ok()));
}

bool GCMStoreImpl::Backend::LoadDeviceCredentials(uint64_t* android_id,
                                                  uint64_t* security_token) {
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;

  std::string result;
  leveldb::Status s = db_->Get(read_options, MakeSlice(kDeviceAIDKey), &result);
  if (s.ok()) {
    if (!base::StringToUint64(result, android_id)) {
      LOG(ERROR) << "Failed to restore device id.";
      return false;
    }
    result.clear();
    s = db_->Get(read_options, MakeSlice(kDeviceTokenKey), &result);
  }
  if (s.ok()) {
    // Mitigate the issues caused by loading DLLs on a background thread
    // (http://crbug/973868).
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

    std::string decrypted_token;
    encryptor_->DecryptString(result, &decrypted_token);
    if (!base::StringToUint64(decrypted_token, security_token)) {
      LOG(ERROR) << "Failed to restore security token.";
      return false;
    }
    return true;
  }

  if (s.IsNotFound()) {
    DVLOG(1) << "No credentials found.";
    return true;
  }

  LOG(ERROR) << "Error reading credentials from store.";
  return false;
}

bool GCMStoreImpl::Backend::LoadRegistrations(
    std::map<std::string, std::string>* registrations) {
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;

  std::unique_ptr<leveldb::Iterator> iter(db_->NewIterator(read_options));
  for (iter->Seek(MakeSlice(kRegistrationKeyStart));
       iter->Valid() && iter->key().ToString() < kRegistrationKeyEnd;
       iter->Next()) {
    leveldb::Slice s = iter->value();
    if (s.size() <= 1) {
      LOG(ERROR) << "Error reading registration with key " << s.ToString();
      return false;
    }
    std::string app_id = ParseRegistrationKey(iter->key().ToString());
    DVLOG(1) << "Found registration with app id " << app_id;
    (*registrations)[app_id] = iter->value().ToString();
  }

  return true;
}

bool GCMStoreImpl::Backend::LoadIncomingMessages(
    std::vector<std::string>* incoming_messages) {
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;

  std::unique_ptr<leveldb::Iterator> iter(db_->NewIterator(read_options));
  std::vector<std::string> expired_incoming_messages;
  for (iter->Seek(MakeSlice(kIncomingMsgKeyStart));
       iter->Valid() && iter->key().ToString() < kIncomingMsgKeyEnd;
       iter->Next()) {
    leveldb::Slice s = iter->value();
    if (s.empty()) {
      LOG(ERROR) << "Error reading incoming message with key "
                 << iter->key().ToString();
      return false;
    }
    DVLOG(1) << "Found incoming message with id " << s.ToString();
    std::string data = s.ToString();
    size_t found = data.find(kIncomingMsgSeparator);
    if (found != std::string::npos) {
      std::string persistent_id = data.substr(0, found);
      int64_t expiration_time = 0LL;
      if (!base::StringToInt64(
              data.substr(found + std::size(kIncomingMsgSeparator) - 1),
              &expiration_time)) {
        LOG(ERROR)
            << "Failed to parse expiration time from the incoming message "
            << data;
        expiration_time = 0LL;
      }
      if (base::Time::Now() < base::Time::FromDeltaSinceWindowsEpoch(
                                  base::Microseconds(expiration_time))) {
        incoming_messages->push_back(std::move(persistent_id));
      } else {
        expired_incoming_messages.push_back(std::move(persistent_id));
      }
    } else {
      if (base::FeatureList::IsEnabled(
              features::kGCMDeleteIncomingMessagesWithoutTTL)) {
        // No expiration time can be found from |data|. The messeage should be
        // added with the legacy non-TTL path. Treat it as expired.
        expired_incoming_messages.push_back(std::move(data));
      } else {
        incoming_messages->push_back(std::move(data));
      }
    }
  }
  if (!expired_incoming_messages.empty()) {
    DVLOG(1) << "Removing " << expired_incoming_messages.size()
             << " expired incoming messages.";
    RemoveIncomingMessages(expired_incoming_messages, base::DoNothing());
  }
  return true;
}

bool GCMStoreImpl::Backend::LoadOutgoingMessages(
    OutgoingMessageMap* outgoing_messages) {
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;

  std::unique_ptr<leveldb::Iterator> iter(db_->NewIterator(read_options));
  for (iter->Seek(MakeSlice(kOutgoingMsgKeyStart));
       iter->Valid() && iter->key().ToString() < kOutgoingMsgKeyEnd;
       iter->Next()) {
    leveldb::Slice s = iter->value();
    if (s.size() <= 1) {
      LOG(ERROR) << "Error reading incoming message with key " << s.ToString();
      return false;
    }
    uint8_t tag = iter->value().data()[0];
    std::string id = ParseOutgoingKey(iter->key().ToString());
    std::unique_ptr<google::protobuf::MessageLite> message(
        BuildProtobufFromTag(tag));
    if (!message.get() ||
        !message->ParseFromString(iter->value().ToString().substr(1))) {
      LOG(ERROR) << "Failed to parse outgoing message with id " << id
                 << " and tag " << tag;
      return false;
    }
    DVLOG(1) << "Found outgoing message with id " << id << " of type "
             << base::NumberToString(tag);
    (*outgoing_messages)[id] = std::move(message);
  }

  return true;
}

bool GCMStoreImpl::Backend::LoadLastCheckinInfo(
    base::Time* last_checkin_time,
    std::set<std::string>* accounts) {
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;

  std::string result;
  leveldb::Status s = db_->Get(read_options,
                               MakeSlice(kLastCheckinTimeKey),
                               &result);
  int64_t time_internal = 0LL;
  if (s.ok() && !base::StringToInt64(result, &time_internal)) {
    LOG(ERROR) << "Failed to restore last checkin time. Using default = 0.";
    time_internal = 0LL;
  }

  // In case we cannot read last checkin time, we default it to 0, as we don't
  // want that situation to cause the whole load to fail.
  *last_checkin_time = base::Time::FromInternalValue(time_internal);

  accounts->clear();
  s = db_->Get(read_options, MakeSlice(kLastCheckinAccountsKey), &result);
  if (!s.ok())
    DVLOG(1) << "No accounts where stored during last run.";

  base::StringTokenizer t(result, ",");
  while (t.GetNext())
    accounts->insert(t.token());

  return true;
}

bool GCMStoreImpl::Backend::LoadGServicesSettings(
    std::map<std::string, std::string>* settings,
    std::string* digest) {
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;

  // Load all of the GServices settings.
  std::unique_ptr<leveldb::Iterator> iter(db_->NewIterator(read_options));
  for (iter->Seek(MakeSlice(kGServiceSettingKeyStart));
       iter->Valid() && iter->key().ToString() < kGServiceSettingKeyEnd;
       iter->Next()) {
    std::string value = iter->value().ToString();
    if (value.empty()) {
      LOG(ERROR) << "Error reading GService Settings " << value;
      return false;
    }
    std::string id = ParseGServiceSettingKey(iter->key().ToString());
    (*settings)[id] = value;
    DVLOG(1) << "Found G Service setting with key: " << id
             << ", and value: " << value;
  }

  // Load the settings digest. It's ok if it is empty.
  db_->Get(read_options, MakeSlice(kGServiceSettingsDigestKey), digest);

  return true;
}

bool GCMStoreImpl::Backend::LoadAccountMappingInfo(
    AccountMappings* account_mappings) {
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;

  AccountMappings loaded_account_mappings;
  std::unique_ptr<leveldb::Iterator> iter(db_->NewIterator(read_options));
  for (iter->Seek(MakeSlice(kAccountKeyStart));
       iter->Valid() && iter->key().ToString() < kAccountKeyEnd;
       iter->Next()) {
    AccountMapping account_mapping;
    account_mapping.account_id = ParseAccountKey(iter->key().ToString());
    if (!account_mapping.ParseFromString(iter->value().ToString())) {
      DVLOG(1) << "Failed to parse account info with ID: "
               << account_mapping.account_id;
      return false;
    }
    DVLOG(1) << "Found account mapping with ID: " << account_mapping.account_id;
    loaded_account_mappings.push_back(account_mapping);
  }

  for (const auto& account_mapping : loaded_account_mappings) {
    account_mappings->push_back(account_mapping);
  }

  return true;
}

bool GCMStoreImpl::Backend::LoadLastTokenFetchTime(
    base::Time* last_token_fetch_time) {
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;

  std::string result;
  leveldb::Status s =
      db_->Get(read_options, MakeSlice(kLastTokenFetchTimeKey), &result);
  int64_t time_internal = 0LL;
  if (s.ok() && !base::StringToInt64(result, &time_internal)) {
    LOG(ERROR) <<
        "Failed to restore last token fetching time. Using default = 0.";
    time_internal = 0LL;
  }

  // In case we cannot read last token fetching time, we default it to 0.
  *last_token_fetch_time = base::Time::FromInternalValue(time_internal);

  return true;
}

bool GCMStoreImpl::Backend::LoadHeartbeatIntervals(
    std::map<std::string, int>* heartbeat_intervals) {
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;

  std::unique_ptr<leveldb::Iterator> iter(db_->NewIterator(read_options));
  for (iter->Seek(MakeSlice(kHeartbeatKeyStart));
       iter->Valid() && iter->key().ToString() < kHeartbeatKeyEnd;
       iter->Next()) {
    std::string scope = ParseHeartbeatKey(iter->key().ToString());
    int interval_ms;
    if (!base::StringToInt(iter->value().ToString(), &interval_ms)) {
      DVLOG(1) << "Failed to parse heartbeat interval info with scope: "
               << scope;
      return false;
    }
    DVLOG(1) << "Found heartbeat interval with scope: " << scope
             << " interval: " << interval_ms << "ms.";
    (*heartbeat_intervals)[scope] = interval_ms;
  }

  return true;
}

bool GCMStoreImpl::Backend::LoadInstanceIDData(
    std::map<std::string, std::string>* instance_id_data) {
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;

  std::unique_ptr<leveldb::Iterator> iter(db_->NewIterator(read_options));
  for (iter->Seek(MakeSlice(kInstanceIDKeyStart));
       iter->Valid() && iter->key().ToString() < kInstanceIDKeyEnd;
       iter->Next()) {
    leveldb::Slice s = iter->value();
    if (s.size() <= 1) {
      LOG(ERROR) << "Error reading IID data with key " << s.ToString();
      return false;
    }
    std::string app_id = ParseInstanceIDKey(iter->key().ToString());
    DVLOG(1) << "Found IID data with app id " << app_id;
    (*instance_id_data)[app_id] = s.ToString();
  }

  return true;
}

GCMStoreImpl::GCMStoreImpl(
    const base::FilePath& path,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    std::unique_ptr<Encryptor> encryptor)
    : backend_(new Backend(path,
                           base::SingleThreadTaskRunner::GetCurrentDefault(),
                           std::move(encryptor))),
      blocking_task_runner_(blocking_task_runner) {}

GCMStoreImpl::~GCMStoreImpl() {
  // |backend_| owns an sql::Database object, which may perform IO when
  // |destroyed.
  blocking_task_runner_->ReleaseSoon(FROM_HERE, std::move(backend_));
}

void GCMStoreImpl::Load(StoreOpenMode open_mode, LoadCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GCMStoreImpl::Backend::Load, backend_, open_mode,
          base::BindOnce(&GCMStoreImpl::LoadContinuation,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void GCMStoreImpl::Close() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  app_message_counts_.clear();
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GCMStoreImpl::Backend::Close, backend_));
}

void GCMStoreImpl::Destroy(UpdateCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GCMStoreImpl::Backend::Destroy, backend_,
                                std::move(callback)));
}

void GCMStoreImpl::SetDeviceCredentials(uint64_t device_android_id,
                                        uint64_t device_security_token,
                                        UpdateCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GCMStoreImpl::Backend::SetDeviceCredentials,
                                backend_, device_android_id,
                                device_security_token, std::move(callback)));
}

void GCMStoreImpl::AddRegistration(const std::string& serialized_key,
                                   const std::string& serialized_value,
                                   UpdateCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMStoreImpl::Backend::AddRegistration, backend_,
                     serialized_key, serialized_value, std::move(callback)));
}

void GCMStoreImpl::RemoveRegistration(const std::string& app_id,
                                      UpdateCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GCMStoreImpl::Backend::RemoveRegistration,
                                backend_, app_id, std::move(callback)));
}

void GCMStoreImpl::AddIncomingMessage(const std::string& persistent_id,
                                      UpdateCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GCMStoreImpl::Backend::AddIncomingMessage,
                                backend_, persistent_id, std::move(callback)));
}

void GCMStoreImpl::RemoveIncomingMessage(const std::string& persistent_id,
                                         UpdateCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMStoreImpl::Backend::RemoveIncomingMessages, backend_,
                     PersistentIdList(1, persistent_id), std::move(callback)));
}

void GCMStoreImpl::RemoveIncomingMessages(
    const PersistentIdList& persistent_ids,
    UpdateCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GCMStoreImpl::Backend::RemoveIncomingMessages,
                                backend_, persistent_ids, std::move(callback)));
}

bool GCMStoreImpl::AddOutgoingMessage(const std::string& persistent_id,
                                      const MCSMessage& message,
                                      UpdateCallback callback) {
  DCHECK_EQ(message.tag(), kDataMessageStanzaTag);
  std::string app_id = reinterpret_cast<const mcs_proto::DataMessageStanza*>(
                           &message.GetProtobuf())->category();
  DCHECK(!app_id.empty());
  if (app_message_counts_.count(app_id) == 0)
    app_message_counts_[app_id] = 0;
  if (app_message_counts_[app_id] < kMessagesPerAppLimit) {
    app_message_counts_[app_id]++;

    blocking_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &GCMStoreImpl::Backend::AddOutgoingMessage, backend_, persistent_id,
            message,
            base::BindOnce(&GCMStoreImpl::AddOutgoingMessageContinuation,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                           app_id)));
    return true;
  }
  return false;
}

void GCMStoreImpl::OverwriteOutgoingMessage(const std::string& persistent_id,
                                            const MCSMessage& message,
                                            UpdateCallback callback) {
  DCHECK_EQ(message.tag(), kDataMessageStanzaTag);
  std::string app_id = reinterpret_cast<const mcs_proto::DataMessageStanza*>(
                           &message.GetProtobuf())->category();
  DCHECK(!app_id.empty());
  // There should already be pending messages for this app.
  DCHECK(app_message_counts_.count(app_id));
  // TODO(zea): consider verifying the specific message already exists.
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMStoreImpl::Backend::AddOutgoingMessage, backend_,
                     persistent_id, message, std::move(callback)));
}

void GCMStoreImpl::RemoveOutgoingMessage(const std::string& persistent_id,
                                         UpdateCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GCMStoreImpl::Backend::RemoveOutgoingMessages, backend_,
          PersistentIdList(1, persistent_id),
          base::BindOnce(&GCMStoreImpl::RemoveOutgoingMessagesContinuation,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void GCMStoreImpl::RemoveOutgoingMessages(
    const PersistentIdList& persistent_ids,
    UpdateCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GCMStoreImpl::Backend::RemoveOutgoingMessages, backend_,
          persistent_ids,
          base::BindOnce(&GCMStoreImpl::RemoveOutgoingMessagesContinuation,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void GCMStoreImpl::SetLastCheckinInfo(const base::Time& time,
                                      const std::set<std::string>& accounts,
                                      UpdateCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GCMStoreImpl::Backend::SetLastCheckinInfo,
                                backend_, time, accounts, std::move(callback)));
}

void GCMStoreImpl::SetGServicesSettings(
    const std::map<std::string, std::string>& settings,
    const std::string& digest,
    UpdateCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMStoreImpl::Backend::SetGServicesSettings, backend_,
                     settings, digest, std::move(callback)));
}

void GCMStoreImpl::AddAccountMapping(const AccountMapping& account_mapping,
                                     UpdateCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMStoreImpl::Backend::AddAccountMapping, backend_,
                     account_mapping, std::move(callback)));
}

void GCMStoreImpl::RemoveAccountMapping(const CoreAccountId& account_id,
                                        UpdateCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GCMStoreImpl::Backend::RemoveAccountMapping,
                                backend_, account_id, std::move(callback)));
}

void GCMStoreImpl::SetLastTokenFetchTime(const base::Time& time,
                                         UpdateCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GCMStoreImpl::Backend::SetLastTokenFetchTime,
                                backend_, time, std::move(callback)));
}

void GCMStoreImpl::AddHeartbeatInterval(const std::string& scope,
                                        int interval_ms,
                                        UpdateCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMStoreImpl::Backend::AddHeartbeatInterval, backend_,
                     scope, interval_ms, std::move(callback)));
}

void GCMStoreImpl::RemoveHeartbeatInterval(const std::string& scope,
                                           UpdateCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GCMStoreImpl::Backend::RemoveHeartbeatInterval,
                                backend_, scope, std::move(callback)));
}

void GCMStoreImpl::SetValueForTesting(const std::string& key,
                                      const std::string& value,
                                      UpdateCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GCMStoreImpl::Backend::SetValue, backend_, key,
                                value, std::move(callback)));
}

void GCMStoreImpl::LoadContinuation(LoadCallback callback,
                                    std::unique_ptr<LoadResult> result) {
  if (!result->success) {
    std::move(callback).Run(std::move(result));
    return;
  }
  for (OutgoingMessageMap::const_iterator
           iter = result->outgoing_messages.begin();
       iter != result->outgoing_messages.end(); ++iter) {
    const mcs_proto::DataMessageStanza* data_message =
        reinterpret_cast<mcs_proto::DataMessageStanza*>(iter->second.get());
    DCHECK(!data_message->category().empty());
    if (app_message_counts_.count(data_message->category()) == 0)
      app_message_counts_[data_message->category()] = 1;
    else
      app_message_counts_[data_message->category()]++;
  }
  std::move(callback).Run(std::move(result));
}

void GCMStoreImpl::AddOutgoingMessageContinuation(UpdateCallback callback,
                                                  const std::string& app_id,
                                                  bool success) {
  if (!success) {
    DCHECK(app_message_counts_[app_id] > 0);
    app_message_counts_[app_id]--;
  }
  std::move(callback).Run(success);
}

void GCMStoreImpl::RemoveOutgoingMessagesContinuation(
    UpdateCallback callback,
    bool success,
    const AppIdToMessageCountMap& removed_message_counts) {
  if (!success) {
    std::move(callback).Run(false);
    return;
  }
  for (AppIdToMessageCountMap::const_iterator iter =
           removed_message_counts.begin();
       iter != removed_message_counts.end(); ++iter) {
    DCHECK_NE(app_message_counts_.count(iter->first), 0U);
    app_message_counts_[iter->first] -= iter->second;
    DCHECK_GE(app_message_counts_[iter->first], 0);
  }
  std::move(callback).Run(true);
}

}  // namespace gcm
