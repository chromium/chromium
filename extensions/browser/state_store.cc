// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/state_store.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/value_store/value_store_factory.h"
#include "extensions/common/extension.h"

namespace {

std::string GetFullKey(const std::string& extension_id,
                       const std::string& key) {
  return extension_id + "." + key;
}

}  // namespace

namespace extensions {

// Helper class to delay tasks until we're ready to start executing them.
class StateStore::DelayedTaskQueue {
 public:
  DelayedTaskQueue() : ready_(false) {}
  ~DelayedTaskQueue() {}

  // Queues up a task for invoking once we're ready. Invokes immediately if
  // we're already ready.
  void InvokeWhenReady(base::OnceClosure task);

  // Marks us ready, and invokes all pending tasks.
  void SetReady();

  // Return whether or not the DelayedTaskQueue is |ready_|.
  bool ready() const { return ready_; }

 private:
  bool ready_;
  std::vector<base::OnceClosure> pending_tasks_;
};

void StateStore::DelayedTaskQueue::InvokeWhenReady(base::OnceClosure task) {
  if (ready_) {
    std::move(task).Run();
  } else {
    pending_tasks_.push_back(std::move(task));
  }
}

void StateStore::DelayedTaskQueue::SetReady() {
  ready_ = true;

  for (base::OnceClosure& task : pending_tasks_)
    std::move(task).Run();
  pending_tasks_.clear();
}

StateStore::StateStore(content::BrowserContext* context,
                       const scoped_refptr<ValueStoreFactory>& store_factory,
                       ValueStoreFrontend::BackendType backend_type,
                       bool deferred_load)
    : store_(
          std::make_unique<ValueStoreFrontend>(store_factory,
                                               backend_type,
                                               GetExtensionFileTaskRunner())),
      task_queue_(std::make_unique<DelayedTaskQueue>()) {
  extension_registry_observation_.Observe(ExtensionRegistry::Get(context));

  if (deferred_load) {
    // Call `Init()` asynchronously with a low priority to not delay startup.
    content::GetUIThreadTaskRunner({base::TaskPriority::USER_VISIBLE})
        ->PostTask(FROM_HERE, base::BindOnce(&StateStore::Init, AsWeakPtr()));
  } else {
    Init();
  }
}

StateStore::~StateStore() {
}

void StateStore::RegisterKey(const std::string& key) {
  registered_keys_.insert(key);
}

void StateStore::GetExtensionValue(const std::string& extension_id,
                                   const std::string& key,
                                   ReadCallback callback) {
  task_queue_->InvokeWhenReady(
      base::BindOnce(&ValueStoreFrontend::Get, base::Unretained(store_.get()),
                     GetFullKey(extension_id, key), std::move(callback)));
}

void StateStore::SetExtensionValue(const std::string& extension_id,
                                   const std::string& key,
                                   std::unique_ptr<base::Value> value) {
  for (TestObserver& observer : observers_)
    observer.WillSetExtensionValue(extension_id, key);

  task_queue_->InvokeWhenReady(
      base::BindOnce(&ValueStoreFrontend::Set, base::Unretained(store_.get()),
                     GetFullKey(extension_id, key), std::move(value)));
}

void StateStore::RemoveExtensionValue(const std::string& extension_id,
                                      const std::string& key) {
  task_queue_->InvokeWhenReady(base::BindOnce(&ValueStoreFrontend::Remove,
                                              base::Unretained(store_.get()),
                                              GetFullKey(extension_id, key)));
}

void StateStore::AddObserver(TestObserver* observer) {
  observers_.AddObserver(observer);
}

void StateStore::RemoveObserver(TestObserver* observer) {
  observers_.RemoveObserver(observer);
}

void StateStore::FlushForTesting(base::OnceClosure flushed_callback) {
  // Look up a key in the database. This serves as a roundtrip to the DB and
  // back; the value of the key doesn't matter.
  GetExtensionValue("fake_id", "fake_key",
                    base::BindOnce(
                        [](base::OnceClosure flushed_callback,
                           std::unique_ptr<base::Value> ignored) {
                          std::move(flushed_callback).Run();
                        },
                        std::move(flushed_callback)));
}

bool StateStore::IsInitialized() const {
  return task_queue_->ready();
}

void StateStore::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update,
    const std::string& old_name) {
  RemoveKeysForExtension(extension->id());
}

void StateStore::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  RemoveKeysForExtension(extension->id());
}

void StateStore::Init() {
  // TODO(cmumford): The store now always lazily initializes upon first access.
  // A follow-on CL will remove this deferred initialization implementation
  // which is now vestigial.
  task_queue_->SetReady();
}

void StateStore::RemoveKeysForExtension(const std::string& extension_id) {
  for (auto key = registered_keys_.begin(); key != registered_keys_.end();
       ++key) {
    task_queue_->InvokeWhenReady(base::BindOnce(
        &ValueStoreFrontend::Remove, base::Unretained(store_.get()),
        GetFullKey(extension_id, *key)));
  }
}

}  // namespace extensions
