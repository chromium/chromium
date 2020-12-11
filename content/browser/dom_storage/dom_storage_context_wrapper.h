// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_WRAPPER_H_
#define CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_WRAPPER_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "components/services/storage/public/mojom/session_storage_control.mojom.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/dom_storage_context.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/dom_storage/session_storage_namespace.mojom.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"

namespace storage {
class SpecialStoragePolicy;
namespace mojom {
class Partition;
}
}

namespace content {

class SessionStorageNamespaceImpl;
class StoragePartitionImpl;

// This is owned by Storage Partition and encapsulates all its dom storage
// state.
// Can only be accessed on the UI thread, except for the AddNamespace and
// RemoveNamespace methods.
class CONTENT_EXPORT DOMStorageContextWrapper
    : public DOMStorageContext,
      public base::RefCountedThreadSafe<DOMStorageContextWrapper> {
 public:
  // Option for PurgeMemory.
  enum PurgeOption {
    // Determines if purging is required based on the usage and the platform.
    PURGE_IF_NEEDED,

    // Purge unopened areas only.
    PURGE_UNOPENED,

    // Purge aggressively, i.e. discard cache even for areas that have
    // non-zero open count.
    PURGE_AGGRESSIVE,
  };

  static scoped_refptr<DOMStorageContextWrapper> Create(
      StoragePartitionImpl* partition,
      storage::SpecialStoragePolicy* special_storage_policy);

  DOMStorageContextWrapper(
      StoragePartitionImpl* partition,
      storage::SpecialStoragePolicy* special_storage_policy);

  storage::mojom::SessionStorageControl* GetSessionStorageControl();
  storage::mojom::LocalStorageControl* GetLocalStorageControl();

  // DOMStorageContext implementation.
  void GetLocalStorageUsage(GetLocalStorageUsageCallback callback) override;
  void GetSessionStorageUsage(GetSessionStorageUsageCallback callback) override;
  void DeleteLocalStorage(const url::Origin& origin,
                          base::OnceClosure callback) override;
  void PerformLocalStorageCleanup(base::OnceClosure callback) override;
  void DeleteSessionStorage(const SessionStorageUsageInfo& usage_info,
                            base::OnceClosure callback) override;
  void PerformSessionStorageCleanup(base::OnceClosure callback) override;
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

  void OpenLocalStorage(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::StorageArea> receiver);
  void BindNamespace(
      const std::string& namespace_id,
      mojo::ReportBadMessageCallback bad_message_callback,
      mojo::PendingReceiver<blink::mojom::SessionStorageNamespace> receiver);
  void BindStorageArea(
      ChildProcessSecurityPolicyImpl::Handle security_policy_handle,
      const url::Origin& origin,
      const std::string& namespace_id,
      mojo::ReportBadMessageCallback bad_message_callback,
      mojo::PendingReceiver<blink::mojom::StorageArea> receiver);

  // Pushes information about known Session Storage namespaces down to the
  // Storage Service instance after a crash. This in turn allows renderer
  // clients to re-establish working connections.
  void RecoverFromStorageServiceCrash();

 private:
  friend class DOMStorageContextWrapperTest;
  friend class base::RefCountedThreadSafe<DOMStorageContextWrapper>;
  friend class SessionStorageNamespaceImpl;  // For MaybeGetExistingNamespace()

  ~DOMStorageContextWrapper() override;

  void MaybeBindSessionStorageControl();
  void MaybeBindLocalStorageControl();
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

  void PurgeMemory(PurgeOption purge_option);

  void OnStartupUsageRetrieved(
      std::vector<storage::mojom::LocalStorageUsageInfoPtr> usage);
  void EnsureLocalStorageOriginIsTracked(const url::Origin& origin);
  void OnStoragePolicyChanged();
  bool ShouldPurgeLocalStorageOnShutdown(const url::Origin& origin);

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

  // Unowned reference to our owning partition. This is always valid until it's
  // reset to null if/when the partition is destroyed. May also be null in
  // tests.
  StoragePartitionImpl* partition_;

  // To receive memory pressure signals.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  // Connections to the partition's Session and Local Storage control interfaces
  // within the Storage Service.
  mojo::Remote<storage::mojom::SessionStorageControl> session_storage_control_;
  mojo::Remote<storage::mojom::LocalStorageControl> local_storage_control_;

  const scoped_refptr<storage::SpecialStoragePolicy> storage_policy_;

  // This wrapper generally lives on the UI thread, but must observe the
  // BrowserContext's SpecialStoragePolicy from the IO thread. This helper does
  // that.
  class StoragePolicyObserver;
  base::SequenceBound<StoragePolicyObserver> storage_policy_observer_;

  // Tracks the total set of origins which may currently have Local Storage data
  // in this partition. This set is synchronized on startup of Local Storage and
  // maintained as new storage areas are bound. This mapping is used to
  // efficiently deduce what policy changes to push to the Local Storage
  // implementation any time a SpecialStoragePolicy change is observed.
  struct LocalStorageOriginState {
    // Indicates that storage for this origin should be purged on shutdown.
    bool should_purge_on_shutdown = false;

    // Indicates the last value for |purge_on_shutdown| communicated to the
    // Local Storage implementation.
    bool will_purge_on_shutdown = false;
  };
  // NOTE: The GURL key is specifically an origin GURL.
  std::map<url::Origin, LocalStorageOriginState> local_storage_origins_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DOMStorageContextWrapper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_CONTEXT_WRAPPER_H_
