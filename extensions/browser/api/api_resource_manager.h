// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_API_RESOURCE_MANAGER_H_
#define EXTENSIONS_BROWSER_API_API_RESOURCE_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"

namespace extensions {

namespace api {
class BluetoothSocketApiFunction;
class BluetoothSocketEventDispatcher;
class SerialConnectFunction;
class SerialPortManager;
class TCPServerSocketEventDispatcher;
class TCPSocketEventDispatcher;
class UDPSocketEventDispatcher;
}  // namespace api

template <typename T>
struct NamedThreadTraits {
  static_assert(T::kThreadId == content::BrowserThread::IO ||
                    T::kThreadId == content::BrowserThread::UI,
                "ApiResources can only belong to the IO or UI thread.");

  static bool IsThreadInitialized() {
    return content::BrowserThread::IsThreadInitialized(T::kThreadId);
  }

  static scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner() {
    return content::BrowserThread::GetTaskRunnerForThread(T::kThreadId);
  }
};

// An ApiResourceManager manages the lifetime of a set of resources that
// that live on named threads (i.e. BrowserThread::IO) which ApiFunctions use.
// Examples of such resources are sockets or USB connections. Note: The only
// named threads that are allowed are the IO and UI threads, since all others
// are deprecated. If we ever need a resource on a different background thread,
// we can modify NamedThreadTraits to be more generic and just return a task
// runner.
//
// Users of this class should define kThreadId to be the thread that
// ApiResourceManager to works on. The default is defined in ApiResource.
// The user must also define a static const char* service_name() that returns
// the name of the service, and in order for ApiResourceManager to use
// service_name() friend this class.
//
// In the cc file the user must define a GetFactoryInstance() and manage their
// own instances (typically using LazyInstance or Singleton).
//
// E.g.:
//
// class Resource {
//  public:
//   static const BrowserThread::ID kThreadId = BrowserThread::IO;
//  private:
//   friend class ApiResourceManager<Resource>;
//   static const char* service_name() {
//     return "ResourceManager";
//    }
// };
//
// In the cc file:
//
// static base::LazyInstance<BrowserContextKeyedAPIFactory<
//     ApiResourceManager<Resource> > >
//         g_factory = LAZY_INSTANCE_INITIALIZER;
//
//
// template <>
// BrowserContextKeyedAPIFactory<ApiResourceManager<Resource> >*
// ApiResourceManager<Resource>::GetFactoryInstance() {
//   return g_factory.Pointer();
// }
template <class T, typename ThreadingTraits = NamedThreadTraits<T>>
class ApiResourceManager : public BrowserContextKeyedAPI,
                           public ExtensionRegistryObserver,
                           public ProcessManagerObserver {
 public:
  explicit ApiResourceManager(content::BrowserContext* context)
      : data_(base::MakeRefCounted<ApiResourceData>()) {
    extension_registry_observation_.Observe(ExtensionRegistry::Get(context));
    process_manager_observation_.Observe(ProcessManager::Get(context));
  }

  virtual ~ApiResourceManager() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(ThreadingTraits::IsThreadInitialized())
        << "A unit test is using an ApiResourceManager but didn't provide "
           "the thread message loop needed for that kind of resource. "
           "Please ensure that the appropriate message loop is operational.";

    data_->InititateCleanup();
  }

  // TODO(lazyboy): Pass unique_ptr<T> instead of T*.
  // Takes ownership.
  int Add(T* api_resource) { return data_->Add(api_resource); }

  void Remove(const ExtensionId& extension_id, int api_resource_id) {
    data_->Remove(extension_id, api_resource_id);
  }

  T* Get(const ExtensionId& extension_id, int api_resource_id) {
    return data_->Get(extension_id, api_resource_id);
  }

  std::unordered_set<int>* GetResourceIds(const ExtensionId& extension_id) {
    return data_->GetResourceIds(extension_id);
  }

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ApiResourceManager<T>>*
  GetFactoryInstance();

  // Convenience method to get the ApiResourceManager for a profile.
  static ApiResourceManager<T>* Get(content::BrowserContext* context) {
    return BrowserContextKeyedAPIFactory<ApiResourceManager<T>>::Get(context);
  }

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return T::service_name(); }

  // Change the resource mapped to this |extension_id| at this
  // |api_resource_id| to |resource|. Returns true and succeeds unless
  // |api_resource_id| does not already identify a resource held by
  // |extension_id|.
  bool Replace(const ExtensionId& extension_id,
               int api_resource_id,
               T* resource) {
    return data_->Replace(extension_id, api_resource_id, resource);
  }

 protected:
  // ProcessManagerObserver:
  void OnBackgroundHostClose(const ExtensionId& extension_id) override {
    data_->InitiateExtensionSuspendedCleanup(extension_id);
  }

  // ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override {
    data_->InitiateExtensionUnloadedCleanup(extension->id());
  }

 private:
  // TODO(rockot): ApiResourceData could be moved out of ApiResourceManager and
  // we could avoid maintaining a friends list here.
  friend class BluetoothAPI;
  friend class api::BluetoothSocketApiFunction;
  friend class api::BluetoothSocketEventDispatcher;
  friend class api::SerialConnectFunction;
  friend class api::SerialPortManager;
  friend class api::TCPServerSocketEventDispatcher;
  friend class api::TCPSocketEventDispatcher;
  friend class api::UDPSocketEventDispatcher;
  friend class BrowserContextKeyedAPIFactory<ApiResourceManager<T>>;

  static const bool kServiceHasOwnInstanceInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  // ApiResourceData class handles resource bookkeeping on a thread
  // where resource lifetime is handled.
  class ApiResourceData : public base::RefCountedThreadSafe<ApiResourceData> {
   public:
    typedef std::map<int, std::unique_ptr<T>> ApiResourceMap;
    // Lookup map from extension id's to allocated resource id's.
    typedef std::map<std::string, std::unordered_set<int>>
        ExtensionToResourceMap;

    ApiResourceData() : next_id_(1) { DETACH_FROM_SEQUENCE(sequence_checker_); }

    // TODO(lazyboy): Pass unique_ptr<T> instead of T*.
    int Add(T* api_resource) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      int id = GenerateId();
      if (id > 0) {
        api_resource_map_[id] = base::WrapUnique<T>(api_resource);

        const ExtensionId& extension_id = api_resource->owner_extension_id();
        ExtensionToResourceMap::iterator it =
            extension_resource_map_.find(extension_id);
        if (it == extension_resource_map_.end()) {
          it = extension_resource_map_
                   .insert(
                       std::make_pair(extension_id, std::unordered_set<int>()))
                   .first;
        }
        it->second.insert(id);
        return id;
      }
      return 0;
    }

    void Remove(const ExtensionId& extension_id, int api_resource_id) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      if (GetOwnedResource(extension_id, api_resource_id)) {
        ExtensionToResourceMap::iterator it =
            extension_resource_map_.find(extension_id);
        it->second.erase(api_resource_id);
        api_resource_map_.erase(api_resource_id);
      }
    }

    T* Get(const ExtensionId& extension_id, int api_resource_id) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return GetOwnedResource(extension_id, api_resource_id);
    }

    // Change the resource mapped to this |extension_id| at this
    // |api_resource_id| to |resource|. Returns true and succeeds unless
    // |api_resource_id| does not already identify a resource held by
    // |extension_id|.
    bool Replace(const ExtensionId& extension_id,
                 int api_resource_id,
                 T* api_resource) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      T* old_resource = api_resource_map_[api_resource_id].get();
      if (old_resource && extension_id == old_resource->owner_extension_id()) {
        api_resource_map_[api_resource_id] = base::WrapUnique<T>(api_resource);
        return true;
      }
      return false;
    }

    std::unordered_set<int>* GetResourceIds(const ExtensionId& extension_id) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return GetOwnedResourceIds(extension_id);
    }

    void InitiateExtensionUnloadedCleanup(const ExtensionId& extension_id) {
      ThreadingTraits::GetSequencedTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &ApiResourceData::CleanupResourcesFromUnloadedExtension, this,
              extension_id));
    }

    void InitiateExtensionSuspendedCleanup(const ExtensionId& extension_id) {
      ThreadingTraits::GetSequencedTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &ApiResourceData::CleanupResourcesFromSuspendedExtension, this,
              extension_id));
    }

    void InititateCleanup() {
      ThreadingTraits::GetSequencedTaskRunner()->PostTask(
          FROM_HERE, base::BindOnce(&ApiResourceData::Cleanup, this));
    }

   private:
    friend class base::RefCountedThreadSafe<ApiResourceData>;

    virtual ~ApiResourceData() {}

    T* GetOwnedResource(const ExtensionId& extension_id, int api_resource_id) {
      const std::unique_ptr<T>& ptr = api_resource_map_[api_resource_id];
      T* resource = ptr.get();
      if (resource && extension_id == resource->owner_extension_id()) {
        return resource;
      }
      return NULL;
    }

    std::unordered_set<int>* GetOwnedResourceIds(
        const ExtensionId& extension_id) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      ExtensionToResourceMap::iterator it =
          extension_resource_map_.find(extension_id);
      if (it == extension_resource_map_.end()) {
        return nullptr;
      }
      return &(it->second);
    }

    void CleanupResourcesFromUnloadedExtension(
        const ExtensionId& extension_id) {
      CleanupResourcesFromExtension(extension_id, true);
    }

    void CleanupResourcesFromSuspendedExtension(
        const ExtensionId& extension_id) {
      CleanupResourcesFromExtension(extension_id, false);
    }

    void CleanupResourcesFromExtension(const ExtensionId& extension_id,
                                       bool remove_all) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

      ExtensionToResourceMap::iterator extension_it =
          extension_resource_map_.find(extension_id);
      if (extension_it == extension_resource_map_.end()) {
        return;
      }

      // Remove all resources, or the non persistent ones only if |remove_all|
      // is false.
      std::unordered_set<int>& resource_ids = extension_it->second;
      for (std::unordered_set<int>::iterator it = resource_ids.begin();
           it != resource_ids.end();) {
        bool erase = false;
        if (remove_all) {
          erase = true;
        } else {
          std::unique_ptr<T>& ptr = api_resource_map_[*it];
          T* resource = ptr.get();
          erase = (resource && !resource->IsPersistent());
        }

        if (erase) {
          api_resource_map_.erase(*it);
          resource_ids.erase(it++);
        } else {
          ++it;
        }
      }  // end for

      // Remove extension entry if we removed all its resources.
      if (resource_ids.size() == 0) {
        extension_resource_map_.erase(extension_id);
      }
    }

    void Cleanup() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

      // Subtle: Move |api_resource_map_| to a temporary and clear that.
      // |api_resource_map_| will become empty and any destructors called
      // transitively from clearing |local_api_resource_map| will see empty
      // |api_resource_map_| instead of trying to access being-destroyed map.
      ApiResourceMap local_api_resource_map;
      api_resource_map_.swap(local_api_resource_map);
      local_api_resource_map.clear();
      // Do the same as above for |extension_resource_map_|.
      ExtensionToResourceMap local_extension_resource_map;
      extension_resource_map_.swap(local_extension_resource_map);
      local_extension_resource_map.clear();
    }

    int GenerateId() { return next_id_++; }

    int next_id_;
    ApiResourceMap api_resource_map_;
    ExtensionToResourceMap extension_resource_map_;
    SEQUENCE_CHECKER(sequence_checker_);
  };

  scoped_refptr<ApiResourceData> data_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
  base::ScopedObservation<ProcessManager, ProcessManagerObserver>
      process_manager_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);
};

template <class T>
struct BrowserContextFactoryDependencies<ApiResourceManager<T>> {
  static void DeclareFactoryDependencies(
      BrowserContextKeyedAPIFactory<ApiResourceManager<T>>* factory) {
    factory->DependsOn(
        ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
    factory->DependsOn(ExtensionRegistryFactory::GetInstance());
    factory->DependsOn(ProcessManagerFactory::GetInstance());
  }
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_API_RESOURCE_MANAGER_H_
