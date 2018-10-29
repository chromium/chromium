// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/storage_frontend.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/api/storage/local_value_store_cache.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/value_store/value_store_factory.h"
#include "extensions/common/api/storage.h"

using content::BrowserContext;
using content::BrowserThread;

namespace extensions {

namespace {

base::LazyInstance<BrowserContextKeyedAPIFactory<StorageFrontend>>::
    DestructorAtExit g_factory = LAZY_INSTANCE_INITIALIZER;

// Settings change Observer which forwards changes on to the extension
// processes for |context| and its incognito partner if it exists.
class DefaultObserver : public SettingsObserver {
 public:
  explicit DefaultObserver(BrowserContext* context)
      : browser_context_(context) {}

  // SettingsObserver implementation.
  void OnSettingsChanged(const std::string& extension_id,
                         settings_namespace::Namespace settings_namespace,
                         const std::string& change_json) override {
    // TODO(gdk): This is a temporary hack while the refactoring for
    // string-based event payloads is removed. http://crbug.com/136045
    std::unique_ptr<base::ListValue> args(new base::ListValue());
    std::unique_ptr<base::Value> changes = base::JSONReader::Read(change_json);
    DCHECK(changes);
    // TODO(devlin): crbug.com/645500 implies this can sometimes fail. If this
    // safeguard fixes it, that means there's an underlying problem (why are we
    // passing invalid json here?).
    if (!changes)
      changes = std::make_unique<base::DictionaryValue>();
    args->Append(std::move(changes));
    args->AppendString(settings_namespace::ToString(settings_namespace));
    std::unique_ptr<Event> event(new Event(events::STORAGE_ON_CHANGED,
                                           api::storage::OnChanged::kEventName,
                                           std::move(args)));
    EventRouter::Get(browser_context_)
        ->DispatchEventToExtension(extension_id, std::move(event));
  }

 private:
  BrowserContext* const browser_context_;
};

}  // namespace

// static
StorageFrontend* StorageFrontend::Get(BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<StorageFrontend>::Get(context);
}

// static
std::unique_ptr<StorageFrontend> StorageFrontend::CreateForTesting(
    scoped_refptr<ValueStoreFactory> storage_factory,
    BrowserContext* context) {
  return base::WrapUnique(
      new StorageFrontend(std::move(storage_factory), context));
}

StorageFrontend::StorageFrontend(BrowserContext* context)
    : StorageFrontend(ExtensionSystem::Get(context)->store_factory(), context) {
}

StorageFrontend::StorageFrontend(scoped_refptr<ValueStoreFactory> factory,
                                 BrowserContext* context)
    : browser_context_(context) {
  Init(std::move(factory));
}

void StorageFrontend::Init(scoped_refptr<ValueStoreFactory> factory) {
  TRACE_EVENT0("browser,startup", "StorageFrontend::Init")
  SCOPED_UMA_HISTOGRAM_TIMER("Extensions.StorageFrontendInitTime");

  observers_ = new SettingsObserverList();
  browser_context_observer_.reset(new DefaultObserver(browser_context_));
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!browser_context_->IsOffTheRecord());

  observers_->AddObserver(browser_context_observer_.get());

  caches_[settings_namespace::LOCAL] = new LocalValueStoreCache(factory);

  // Add any additional caches the embedder supports (for example, caches
  // for chrome.storage.managed and chrome.storage.sync).
  ExtensionsAPIClient::Get()->AddAdditionalValueStoreCaches(
      browser_context_, factory, observers_, &caches_);
}

StorageFrontend::~StorageFrontend() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_->RemoveObserver(browser_context_observer_.get());
  for (auto it = caches_.begin(); it != caches_.end(); ++it) {
    ValueStoreCache* cache = it->second;
    cache->ShutdownOnUI();
    GetBackendTaskRunner()->DeleteSoon(FROM_HERE, cache);
  }
}

ValueStoreCache* StorageFrontend::GetValueStoreCache(
    settings_namespace::Namespace settings_namespace) const {
  auto it = caches_.find(settings_namespace);
  if (it != caches_.end())
    return it->second;
  return NULL;
}

bool StorageFrontend::IsStorageEnabled(
    settings_namespace::Namespace settings_namespace) const {
  return caches_.find(settings_namespace) != caches_.end();
}

void StorageFrontend::RunWithStorage(
    scoped_refptr<const Extension> extension,
    settings_namespace::Namespace settings_namespace,
    const ValueStoreCache::StorageCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(extension.get());

  ValueStoreCache* cache = caches_[settings_namespace];
  CHECK(cache);

  GetBackendTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ValueStoreCache::RunWithValueStoreForExtension,
                            base::Unretained(cache), callback, extension));
}

void StorageFrontend::DeleteStorageSoon(const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto it = caches_.begin(); it != caches_.end(); ++it) {
    ValueStoreCache* cache = it->second;
    GetBackendTaskRunner()->PostTask(
        FROM_HERE, base::Bind(&ValueStoreCache::DeleteStorageSoon,
                              base::Unretained(cache), extension_id));
  }
}

scoped_refptr<SettingsObserverList> StorageFrontend::GetObservers() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return observers_;
}

void StorageFrontend::DisableStorageForTesting(
    settings_namespace::Namespace settings_namespace) {
  auto it = caches_.find(settings_namespace);
  if (it != caches_.end()) {
    ValueStoreCache* cache = it->second;
    cache->ShutdownOnUI();
    GetBackendTaskRunner()->DeleteSoon(FROM_HERE, cache);
    caches_.erase(it);
  }
}

// BrowserContextKeyedAPI implementation.

// static
BrowserContextKeyedAPIFactory<StorageFrontend>*
StorageFrontend::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
const char* StorageFrontend::service_name() { return "StorageFrontend"; }

}  // namespace extensions
