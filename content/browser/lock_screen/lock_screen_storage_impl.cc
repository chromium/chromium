// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/lock_screen/lock_screen_storage_impl.h"

#include <map>
#include <memory>

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "components/value_store/value_store.h"
#include "components/value_store/value_store_factory.h"
#include "components/value_store/value_store_factory_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "url/origin.h"

using value_store::ValueStore;

namespace content {

namespace {

// See Extensions.Database.Open in histograms.xml.
const char kValueStoreDatabaseUMAClientName[] = "WebAppsLockScreen";

}  // namespace

// Helper class for running blocking tasks on a thread pool.
class LockScreenStorageHelper {
 public:
  LockScreenStorageHelper();
  ~LockScreenStorageHelper() = default;

  void Init(const base::FilePath& base_path);
  std::vector<std::string> GetKeys(const url::Origin& origin);
  bool SetData(const url::Origin& origin,
               const std::string& key,
               const std::string& data);

 private:
  ValueStore* GetValueStoreForOrigin(const url::Origin& origin);

  scoped_refptr<value_store::ValueStoreFactory> value_store_factory_;
  // Maps storage directory filename to ValueStore for a particular origin.
  // TODO(crbug.com/40204655): If there can only be one lock screen app at a
  // time, this does not need to be a map. Otherwise, there should be a clean
  // way of evicting value stores databases from this cache.
  std::map<std::string, std::unique_ptr<ValueStore>> storage_map_;
};

LockScreenStorageHelper::LockScreenStorageHelper() {}

void LockScreenStorageHelper::Init(const base::FilePath& base_path) {
  value_store_factory_ =
      base::MakeRefCounted<value_store::ValueStoreFactoryImpl>(base_path);
}

std::vector<std::string> LockScreenStorageHelper::GetKeys(
    const url::Origin& origin) {
  ValueStore* value_store = GetValueStoreForOrigin(origin);
  ValueStore::ReadResult read = value_store->Get();
  std::vector<std::string> result;
  if (!read.status().ok())
    return result;
  for (auto kv : read.settings()) {
    result.push_back(kv.first);
  }

  return result;
}

bool LockScreenStorageHelper::SetData(const url::Origin& origin,
                                      const std::string& key,
                                      const std::string& data) {
  ValueStore* value_store = GetValueStoreForOrigin(origin);
  ValueStore::WriteResult write =
      value_store->Set(ValueStore::DEFAULTS, key, base::Value(data));
  return write.status().ok();
}

ValueStore* LockScreenStorageHelper::GetValueStoreForOrigin(
    const url::Origin& origin) {
  DCHECK(!origin.opaque());

  // ValueStore will create a directory for storing its data. The directory name
  // is passed in. We want to key data by origin, so we use a hash of the origin
  // as the directory name under which to store the data. Origin.Serialize()
  // should just concatenate the scheme/host/port, which are the components that
  // need to appear identical if the two origins need to compare equal. Hence
  // if two origins are equal, the serialized origins should also be equal.
  std::string serialized_origin = origin.Serialize();
  uint8_t hash[crypto::kSHA256Length];
  crypto::SHA256HashString(serialized_origin, hash, sizeof(hash));
  std::string filename = base::HexEncode(hash);

  auto iter = storage_map_.find(filename);
  if (iter != storage_map_.end())
    return iter->second.get();

  base::FilePath value_store_path(filename);
  std::unique_ptr<ValueStore> value_store =
      value_store_factory_->CreateValueStore(value_store_path,
                                             kValueStoreDatabaseUMAClientName);
  ValueStore* result = value_store.get();
  storage_map_.emplace(filename, std::move(value_store));
  return result;
}

// static
LockScreenStorage* LockScreenStorage::GetInstance() {
  return LockScreenStorageImpl::GetInstance();
}

// static
LockScreenStorageImpl* LockScreenStorageImpl::GetInstance() {
  return base::Singleton<
      LockScreenStorageImpl,
      base::LeakySingletonTraits<LockScreenStorageImpl>>::get();
}

LockScreenStorageImpl::LockScreenStorageImpl()
    : helper_(base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {
}

LockScreenStorageImpl::~LockScreenStorageImpl() = default;

void LockScreenStorageImpl::Init(content::BrowserContext* browser_context,
                                 const base::FilePath& base_path) {
  DCHECK(!browser_context_);
  DCHECK(!browser_context->IsOffTheRecord());
  browser_context_ = browser_context;
  helper_.AsyncCall(&LockScreenStorageHelper::Init).WithArgs(base_path);
}

void LockScreenStorageImpl::GetKeys(
    const url::Origin& origin,
    blink::mojom::LockScreenService::GetKeysCallback callback) {
  helper_.AsyncCall(&LockScreenStorageHelper::GetKeys)
      .WithArgs(origin)
      .Then(base::BindOnce(&LockScreenStorageImpl::OnGetKeys,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LockScreenStorageImpl::SetData(
    const url::Origin& origin,
    const std::string& key,
    const std::string& data,
    blink::mojom::LockScreenService::SetDataCallback callback) {
  helper_.AsyncCall(&LockScreenStorageHelper::SetData)
      .WithArgs(origin, key, data)
      .Then(base::BindOnce(&LockScreenStorageImpl::OnSetData,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

bool LockScreenStorageImpl::IsAllowedBrowserContext(
    content::BrowserContext* browser_context) {
  return browser_context == browser_context_;
}

void LockScreenStorageImpl::OnGetKeys(
    blink::mojom::LockScreenService::GetKeysCallback callback,
    const std::vector<std::string>& result) {
  std::move(callback).Run(result);
}

void LockScreenStorageImpl::OnSetData(
    blink::mojom::LockScreenService::SetDataCallback callback,
    bool success) {
  if (success) {
    std::move(callback).Run(blink::mojom::LockScreenServiceStatus::kSuccess);
  } else {
    std::move(callback).Run(blink::mojom::LockScreenServiceStatus::kWriteError);
  }
}

void LockScreenStorageImpl::InitForTesting(
    content::BrowserContext* browser_context,
    const base::FilePath& base_path) {
  browser_context_ = nullptr;
  Init(browser_context, base_path);
}

}  // namespace content
