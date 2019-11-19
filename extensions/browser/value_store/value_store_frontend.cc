// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/value_store/value_store_frontend.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/value_store/leveldb_value_store.h"
#include "extensions/browser/value_store/value_store_factory.h"

using content::BrowserThread;
using extensions::ValueStoreFactory;
using extensions::IsOnBackendSequence;
using extensions::GetBackendTaskRunner;

class ValueStoreFrontend::Backend : public base::RefCountedThreadSafe<Backend> {
 public:
  Backend(const scoped_refptr<ValueStoreFactory>& store_factory,
          BackendType backend_type)
      : store_factory_(store_factory), backend_type_(backend_type) {}

  void Get(const std::string& key,
           const ValueStoreFrontend::ReadCallback& callback) {
    DCHECK(IsOnBackendSequence());
    LazyInit();
    ValueStore::ReadResult result = storage_->Get(key);

    // Extract the value from the ReadResult and pass ownership of it to the
    // callback.
    std::unique_ptr<base::Value> value;
    if (result.status().ok()) {
      result.settings().RemoveWithoutPathExpansion(key, &value);
    } else {
      LOG(WARNING) << "Reading " << key << " from " << db_path_.value()
                   << " failed: " << result.status().message;
    }

    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&ValueStoreFrontend::Backend::RunCallback,
                                  this, callback, std::move(value)));
  }

  void Set(const std::string& key, std::unique_ptr<base::Value> value) {
    DCHECK(IsOnBackendSequence());
    LazyInit();
    // We don't need the old value, so skip generating changes.
    ValueStore::WriteResult result = storage_->Set(
        ValueStore::IGNORE_QUOTA | ValueStore::NO_GENERATE_CHANGES, key,
        *value);
    LOG_IF(ERROR, !result.status().ok())
        << "Error while writing " << key << " to " << db_path_.value();
  }

  void Remove(const std::string& key) {
    DCHECK(IsOnBackendSequence());
    LazyInit();
    storage_->Remove(key);
  }

 private:
  friend class base::RefCountedThreadSafe<Backend>;

  virtual ~Backend() {
    if (storage_ && !IsOnBackendSequence())
      GetBackendTaskRunner()->DeleteSoon(FROM_HERE, storage_.release());
  }

  void LazyInit() {
    DCHECK(IsOnBackendSequence());
    if (storage_)
      return;
    TRACE_EVENT0("ValueStoreFrontend::Backend", "LazyInit");
    switch (backend_type_) {
      case BackendType::RULES:
        storage_ = store_factory_->CreateRulesStore();
        break;
      case BackendType::STATE:
        storage_ = store_factory_->CreateStateStore();
        break;
    }
  }

  void RunCallback(const ValueStoreFrontend::ReadCallback& callback,
                   std::unique_ptr<base::Value> value) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    callback.Run(std::move(value));
  }

  // The factory which will be used to lazily create the ValueStore when needed.
  // Used exclusively on the backend sequence.
  scoped_refptr<ValueStoreFactory> store_factory_;
  BackendType backend_type_;

  // The actual ValueStore that handles persisting the data to disk. Used
  // exclusively on the backend sequence.
  std::unique_ptr<ValueStore> storage_;

  base::FilePath db_path_;

  DISALLOW_COPY_AND_ASSIGN(Backend);
};

ValueStoreFrontend::ValueStoreFrontend(
    const scoped_refptr<ValueStoreFactory>& store_factory,
    BackendType backend_type)
    : backend_(base::MakeRefCounted<Backend>(store_factory, backend_type)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ValueStoreFrontend::~ValueStoreFrontend() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ValueStoreFrontend::Get(const std::string& key,
                             const ReadCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GetBackendTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ValueStoreFrontend::Backend::Get, backend_,
                                key, callback));
}

void ValueStoreFrontend::Set(const std::string& key,
                             std::unique_ptr<base::Value> value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GetBackendTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ValueStoreFrontend::Backend::Set, backend_,
                                key, std::move(value)));
}

void ValueStoreFrontend::Remove(const std::string& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GetBackendTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ValueStoreFrontend::Backend::Remove, backend_, key));
}
