// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_OBSERVER_HUB_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_OBSERVER_HUB_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_info.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

// Type-safe per-instance observer hub for a specific service process.
//
// The template parameter is the service's mojom interface (e.g.
// audio::mojom::AudioService), providing compile-time safety — observers
// of one service can't accidentally register with another service's hub.
//
// The caller that launches the service should own this instance, tying
// its lifetime to the service remote.
//
// Usage:
//   ServiceProcessObserverHub<audio::mojom::AudioService> hub_;
//   // At launch:
//   options.WithObserver(hub_.AsWeakPtr());
//   // Observers:
//   hub_.AddObserver(listener);
template <typename ServiceInterface>
class ServiceProcessObserverHub : public ServiceProcessHost::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnServiceLaunched(const ServiceProcessInfo& info) {}
    virtual void OnServiceTerminatedNormally(const ServiceProcessInfo& info) {}
    virtual void OnServiceCrashed(const ServiceProcessInfo& info) {}
  };

  ServiceProcessObserverHub() = default;
  ~ServiceProcessObserverHub() override = default;

  ServiceProcessObserverHub(const ServiceProcessObserverHub&) = delete;
  ServiceProcessObserverHub& operator=(const ServiceProcessObserverHub&) =
      delete;

  void AddObserver(Observer* observer) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    observers_.AddObserver(observer);
    if (current_process_info_) {
      observer->OnServiceLaunched(*current_process_info_);
    }
  }

  void RemoveObserver(Observer* observer) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    observers_.RemoveObserver(observer);
  }

  base::WeakPtr<ServiceProcessHost::Observer> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void OnServiceProcessLaunched(const ServiceProcessInfo& info) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    current_process_info_ = info.Duplicate();
    for (auto& observer : observers_) {
      observer.OnServiceLaunched(info);
    }
  }

  void OnServiceProcessTerminatedNormally(
      const ServiceProcessInfo& info) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    current_process_info_.reset();
    for (auto& observer : observers_) {
      observer.OnServiceTerminatedNormally(info);
    }
  }

  void OnServiceProcessCrashed(const ServiceProcessInfo& info) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    current_process_info_.reset();
    for (auto& observer : observers_) {
      observer.OnServiceCrashed(info);
    }
  }

  std::optional<ServiceProcessInfo> current_process_info_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<ServiceProcessObserverHub> weak_factory_{this};
};

// Pairs a service remote with its observer hub, with lifetime tied to the
// UI-thread sequence via SequenceLocalStorageSlot. Supports teardown and
// re-creation between unit tests.
template <typename ServiceInterface>
struct ServiceProcessState {
  mojo::Remote<ServiceInterface> remote;
  ServiceProcessObserverHub<ServiceInterface> observer_hub;

  static ServiceProcessState& GetOrCreate() {
    static base::SequenceLocalStorageSlot<ServiceProcessState> slot;
    return slot.GetOrCreateValue();
  }
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_OBSERVER_HUB_H_
