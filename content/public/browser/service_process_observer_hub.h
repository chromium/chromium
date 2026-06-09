// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_OBSERVER_HUB_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_OBSERVER_HUB_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_info.h"

namespace content {

// Type-safe per-instance observer hub for a specific service process.
//
// The template parameter is the service's mojom interface (e.g.
// audio::mojom::AudioService), providing compile-time safety — observers
// of one service can't accidentally register with another service's hub.
//
// Prefer using ObservedServiceRemote<T> which pairs this hub with a
// mojo::Remote and integrates with ServiceProcessHost::Launch.
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
  // NOTE ON ORDERING: When a service process dies, two mojo disconnect
  // callbacks are posted to the UI thread independently:
  //   1. The service interface pipe disconnect (e.g. reset_on_disconnect on
  //      the audio::mojom::AudioService remote)
  //   2. The child process host channel disconnect (triggers
  //      OnChildDisconnected → NotifyCrashed → this observer)
  //
  // Their relative ordering is NOT guaranteed. If (1) fires first, the
  // caller may relaunch the service (calling AsWeakPtr() and receiving a new
  // OnServiceProcessLaunched) before (2) delivers the crash notification for
  // the OLD instance. The !current_process_info_ guards below handle this
  // by silently dropping stale terminate/crash notifications that arrive
  // after the hub has already moved on to a new instance.

  void OnServiceProcessLaunched(const ServiceProcessInfo& info) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    CHECK(!current_process_info_ ||
          current_process_info_->service_process_id() !=
              info.service_process_id())
        << "Duplicate OnServiceProcessLaunched for same service_process_id";
    current_process_info_ = info.Duplicate();
    for (auto& observer : observers_) {
      observer.OnServiceLaunched(info);
    }
  }

  void OnServiceProcessTerminatedNormally(
      const ServiceProcessInfo& info) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!current_process_info_ || current_process_info_->service_process_id() !=
                                      info.service_process_id()) {
      return;
    }
    current_process_info_.reset();
    for (auto& observer : observers_) {
      observer.OnServiceTerminatedNormally(info);
    }
  }

  void OnServiceProcessCrashed(const ServiceProcessInfo& info) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!current_process_info_ || current_process_info_->service_process_id() !=
                                      info.service_process_id()) {
      return;
    }
    current_process_info_.reset();
    for (auto& observer : observers_) {
      observer.OnServiceCrashed(info);
    }
  }

  std::optional<ServiceProcessInfo> current_process_info_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<ServiceProcessObserverHub> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_OBSERVER_HUB_H_
