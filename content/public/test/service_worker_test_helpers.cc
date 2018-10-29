// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/service_worker_test_helpers.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace content {

namespace {

class StoppedObserver : public base::RefCountedThreadSafe<StoppedObserver> {
 public:
  static void StartObserving(ServiceWorkerContextWrapper* context,
                             int64_t service_worker_version_id,
                             base::OnceClosure completion_callback_ui) {
    auto observer = base::WrapRefCounted(
        new StoppedObserver(std::move(completion_callback_ui)));
    // Adds a ref to StoppedObserver to keep |this| around until the worker is
    // stopped.
    observer->inner_observer_ = std::make_unique<Observer>(
        context, service_worker_version_id,
        base::BindOnce(&StoppedObserver::OnStopped, observer));
  }

 private:
  friend class base::RefCountedThreadSafe<StoppedObserver>;

  explicit StoppedObserver(base::OnceClosure completion_callback_ui)
      : completion_callback_ui_(std::move(completion_callback_ui)) {}
  ~StoppedObserver() {}

  class Observer : public ServiceWorkerContextCoreObserver {
   public:
    Observer(ServiceWorkerContextWrapper* context,
             int64_t service_worker_version_id,
             base::OnceClosure stopped_callback)
        : context_(context),
          version_id_(service_worker_version_id),
          stopped_callback_(std::move(stopped_callback)) {
      context_->AddObserver(this);
    }

    // ServiceWorkerContextCoreObserver:
    void OnRunningStateChanged(int64_t version_id,
                               EmbeddedWorkerStatus status) override {
      DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
      if (version_id != version_id_ || status != EmbeddedWorkerStatus::STOPPED)
        return;
      std::move(stopped_callback_).Run();
    }
    ~Observer() override { context_->RemoveObserver(this); }

   private:
    ServiceWorkerContextWrapper* const context_;
    int64_t version_id_;
    base::OnceClosure stopped_callback_;
  };

  void OnStopped() {
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(&StoppedObserver::OnStopped, this));
      return;
    }
    std::move(completion_callback_ui_).Run();
  }

  std::unique_ptr<Observer> inner_observer_;
  base::OnceClosure completion_callback_ui_;

  DISALLOW_COPY_AND_ASSIGN(StoppedObserver);
};

void FoundReadyRegistration(
    ServiceWorkerContextWrapper* context_wrapper,
    base::OnceClosure completion_callback,
    blink::ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK_EQ(blink::ServiceWorkerStatusCode::kOk, service_worker_status);
  int64_t version_id =
      service_worker_registration->active_version()->version_id();
  StoppedObserver::StartObserving(context_wrapper, version_id,
                                  std::move(completion_callback));
  service_worker_registration->active_version()->embedded_worker()->Stop();
}

}  // namespace

void StopServiceWorkerForScope(ServiceWorkerContext* context,
                               const GURL& scope,
                               base::OnceClosure completion_callback_ui) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&StopServiceWorkerForScope, context, scope,
                       std::move(completion_callback_ui)));
    return;
  }
  auto* context_wrapper = static_cast<ServiceWorkerContextWrapper*>(context);
  context_wrapper->FindReadyRegistrationForScope(
      scope, base::BindOnce(&FoundReadyRegistration,
                            base::RetainedRef(context_wrapper),
                            std::move(completion_callback_ui)));
}

}  // namespace content
