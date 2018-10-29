// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_WRAPPER_H_
#define CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_WRAPPER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "components/services/leveldb/public/interfaces/leveldb.mojom.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/dom_storage_context.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/mojom/dom_storage/session_storage_namespace.mojom.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"
#include "url/origin.h"

namespace base {
class FilePath;
}

namespace service_manager {
class Connector;
}

namespace storage {
class SpecialStoragePolicy;
}

namespace content {

class DOMStorageContextImpl;
class LocalStorageContextMojo;
class SessionStorageContextMojo;
class SessionStorageNamespaceImpl;

// This is owned by Storage Partition and encapsulates all its dom storage
// state.
class CONTENT_EXPORT DOMStorageContextWrapper
    : public DOMStorageContext,
      public base::RefCountedThreadSafe<DOMStorageContextWrapper> {
 public:
  // If |data_path| is empty, nothing will be saved to disk.
  DOMStorageContextWrapper(
      service_manager::Connector* connector,
      const base::FilePath& data_path,
      const base::FilePath& local_partition_path,
      storage::SpecialStoragePolicy* special_storage_policy);

  // DOMStorageContext implementation.
  void GetLocalStorageUsage(GetLocalStorageUsageCallback callback) override;
  void GetSessionStorageUsage(GetSessionStorageUsageCallback callback) override;
  void DeleteLocalStorage(const GURL& origin,
                          base::OnceClosure callback) override;
  void PerformLocalStorageCleanup(base::OnceClosure callback) override;
  void DeleteSessionStorage(const SessionStorageUsageInfo& usage_info) override;
  void SetSaveSessionStorageOnDisk() override;
  scoped_refptr<SessionStorageNamespace> RecreateSessionStorage(
      const std::string& namespace_id) override;
  void StartScavengingUnusedSessionStorage() override;

  // Used by content settings to alter the behavior around
  // what data to keep and what data to discard at shutdown.
  // The policy is not so straight forward to describe, see
  // the implementation for details.
  void SetForceKeepSessionState();

  // Called when the BrowserContext/Profile is going away.
  void Shutdown();

  void Flush();

  // See mojom::StoragePartitionService interface.
  void OpenLocalStorage(const url::Origin& origin,
                        blink::mojom::StorageAreaRequest request);
  void OpenSessionStorage(int process_id,
                          const std::string& namespace_id,
                          mojo::ReportBadMessageCallback bad_message_callback,
                          blink::mojom::SessionStorageNamespaceRequest request);

  void SetLocalStorageDatabaseForTesting(
      leveldb::mojom::LevelDBDatabaseAssociatedPtr database);

  SessionStorageContextMojo* mojo_session_state() {
    return mojo_session_state_;
  }

 private:
  friend class DOMStorageMessageFilter;  // for access to context()
  friend class SessionStorageNamespaceImpl;  // ditto
  friend class base::RefCountedThreadSafe<DOMStorageContextWrapper>;
  friend class DOMStorageBrowserTest;

  ~DOMStorageContextWrapper() override;
  DOMStorageContextImpl* context() const { return context_.get(); }

  base::SequencedTaskRunner* mojo_task_runner() {
    return mojo_task_runner_.get();
  }

  scoped_refptr<SessionStorageNamespaceImpl> MaybeGetExistingNamespace(
      const std::string& namespace_id) const;

  // Note: can be called on multiple threads, protected by a mutex.
  void AddNamespace(const std::string& namespace_id,
                    SessionStorageNamespaceImpl* session_namespace);

  // Note: can be called on multiple threads, protected by a mutex.
  void RemoveNamespace(const std::string& namespace_id);

  // Called on UI thread when the system is under memory pressure.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  void PurgeMemory(DOMStorageContextImpl::PurgeOption purge_option);

  // Keep all mojo-ish details together and not bleed them through the public
  // interface. The |mojo_state_| object is owned by this object, but destroyed
  // asynchronously on the |mojo_task_runner_|.
  LocalStorageContextMojo* mojo_state_ = nullptr;
  SessionStorageContextMojo* mojo_session_state_ = nullptr;
  scoped_refptr<base::SequencedTaskRunner> mojo_task_runner_;

  // Since the tab restore code keeps a reference to the session namespaces
  // of recently closed tabs (see sessions::ContentPlatformSpecificTabData and
  // sessions::TabRestoreService), a SessionStorageNamespaceImpl can outlive the
  // destruction of the browser window. A session restore can also happen
  // without the browser context being shutdown or destroyed in between. The
  // design of SessionStorageNamespaceImpl assumes there is only one object per
  // namespace. A session restore creates new objects for all tabs while the
  // Profile wasn't destructed. This map allows the restored session to re-use
  // the SessionStorageNamespaceImpl objects that are still alive thanks to the
  // sessions component.
  std::map<std::string, SessionStorageNamespaceImpl*> alive_namespaces_
      GUARDED_BY(alive_namespaces_lock_);
  mutable base::Lock alive_namespaces_lock_;

  base::FilePath legacy_localstorage_path_;

  // To receive memory pressure signals.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  scoped_refptr<DOMStorageContextImpl> context_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DOMStorageContextWrapper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_WRAPPER_H_
