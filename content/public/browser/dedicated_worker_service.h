// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEDICATED_WORKER_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_DEDICATED_WORKER_SERVICE_H_

#include "base/observer_list_types.h"
#include "content/common/content_export.h"
#include "content/public/browser/dedicated_worker_creator.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/common/tokens/tokens.h"

class GURL;

namespace url {
class Origin;
}

namespace content {

// An interface that allows to subscribe to the lifetime of dedicated workers.
// The service is owned by the StoragePartition and lives on the UI thread.
class CONTENT_EXPORT DedicatedWorkerService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when a dedicated worker is created/destroyed. Note that it is not
    // yet started in the renderer since its script still has to be downloaded
    // and evaluated.
    virtual void OnWorkerCreated(
        const blink::DedicatedWorkerToken& worker_token,
        int worker_process_id,
        const url::Origin& security_origin,
        DedicatedWorkerCreator creator) = 0;
    virtual void OnBeforeWorkerDestroyed(
        const blink::DedicatedWorkerToken& worker_token,
        DedicatedWorkerCreator creator) = 0;

    // Called when the final response URL (the URL after redirects) was
    // determined when fetching the worker's script.
    virtual void OnFinalResponseURLDetermined(
        const blink::DedicatedWorkerToken& worker_token,
        const GURL& url) = 0;
  };

  // Adds/removes an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Invokes OnWorkerCreated() on |observer| for all existing dedicated workers.
  //
  // This function must be invoked in conjunction with AddObserver(). It is
  // meant to be used by an observer that dynamically subscribes to the
  // DedicatedWorkerService while some workers are already running. It avoids
  // receiving a OnBeforeWorkerDestroyed() event without having received the
  // corresponding OnWorkerCreated() event.
  virtual void EnumerateDedicatedWorkers(Observer* observer) = 0;

 protected:
  virtual ~DedicatedWorkerService() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEDICATED_WORKER_SERVICE_H_
