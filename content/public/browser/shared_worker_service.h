// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SHARED_WORKER_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_SHARED_WORKER_SERVICE_H_

#include <string>

#include "base/observer_list_types.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/worker/shared_worker_info.mojom.h"

class GURL;

namespace base {
class UnguessableToken;
}

namespace blink {
class StorageKey;
}  // namespace blink

namespace url {
class Origin;
}

namespace content {

// An interface for managing shared workers. These may be run in a separate
// process, since multiple renderer processes can be talking to a single shared
// worker. All the methods below can only be called on the UI thread.
class CONTENT_EXPORT SharedWorkerService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when a shared worker is created/destroyed. Note that it is not yet
    // started in the renderer since its script still has to be downloaded and
    // evaluated.
    virtual void OnWorkerCreated(
        const blink::SharedWorkerToken& token,
        int worker_process_id,
        const url::Origin& security_origin,
        const base::UnguessableToken& dev_tools_token) = 0;
    virtual void OnBeforeWorkerDestroyed(
        const blink::SharedWorkerToken& token) = 0;

    // Called when the final response URL (the URL after redirects) was
    // determined when fetching the worker's script.
    //
    // TODO(pmonette): Implement this in derived classes and make it pure.
    virtual void OnFinalResponseURLDetermined(
        const blink::SharedWorkerToken& token,
        const GURL& url) {}

    // Called when a frame starts/stop being a client of a shared worker. It is
    // guaranteed that OnWorkerCreated() is called before receiving these
    // notifications.
    virtual void OnClientAdded(
        const blink::SharedWorkerToken& token,
        content::GlobalRenderFrameHostId render_frame_host_id) = 0;
    virtual void OnClientRemoved(
        const blink::SharedWorkerToken& token,
        content::GlobalRenderFrameHostId render_frame_host_id) = 0;
  };

  // Adds/removes an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Invokes OnWorkerCreated() on |observer| for all existing shared workers.
  //
  // This function must be invoked in conjunction with AddObserver(). It is
  // meant to be used by an observer that dynamically subscribe to the
  // SharedWorkerService while some workers already exist. It avoids
  // receiving a OnBeforeWorkerDestroyed() event without having received the
  // corresponding OnWorkerCreated() event.
  //
  // Note: Due to current callers not needing it, this function does NOT call
  //       OnClientAdded() for each worker's clients.
  virtual void EnumerateSharedWorkers(Observer* observer) = 0;

  // Terminates the given shared worker identified by its name, the URL of its
  // main script resource, the storage key, and the same_site_cookies setting.
  // Returns true on success.
  virtual bool TerminateWorker(
      const GURL& url,
      const std::string& name,
      const blink::StorageKey& storage_key,
      const blink::mojom::SharedWorkerSameSiteCookies same_site_cookies) = 0;

  // Drops all shared workers and references to processes for shared workers
  // synchronously.
  virtual void Shutdown() = 0;

 protected:
  virtual ~SharedWorkerService() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SHARED_WORKER_SERVICE_H_
