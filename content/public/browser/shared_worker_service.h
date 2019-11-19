// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SHARED_WORKER_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_SHARED_WORKER_SERVICE_H_

#include <string>

#include "base/observer_list_types.h"
#include "content/common/content_export.h"

class GURL;

namespace base {
class UnguessableToken;
}

namespace url {
class Origin;
}

namespace content {

class SharedWorkerInstance;

// An interface for managing shared workers. These may be run in a separate
// process, since multiple renderer processes can be talking to a single shared
// worker. All the methods below can only be called on the UI thread.
class CONTENT_EXPORT SharedWorkerService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when a shared worker has started/stopped. This means that Start()
    // was called for that worker and it got assigned its DevTools token. Note
    // that it may still be evaluating the script and thus it could not be yet
    // running in the renderer. This differs a bit from the "started" state of
    // the embedded worker.
    virtual void OnWorkerStarted(
        const SharedWorkerInstance& instance,
        int worker_process_id,
        const base::UnguessableToken& dev_tools_token) = 0;
    virtual void OnBeforeWorkerTerminated(
        const SharedWorkerInstance& instance) = 0;

    // Called when a frame starts/stop being a client of a shared worker. It is
    // guaranteed that OnWorkerStarted() is called before receiving these
    // notifications.
    virtual void OnClientAdded(const SharedWorkerInstance& instance,
                               int client_process_id,
                               int frame_id) = 0;
    virtual void OnClientRemoved(const SharedWorkerInstance& instance,
                                 int client_process_id,
                                 int frame_id) = 0;
  };

  // Adds/removes an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Terminates the given shared worker identified by its name, the URL of
  // its main script resource, and the constructor origin. Returns true on
  // success.
  virtual bool TerminateWorker(const GURL& url,
                               const std::string& name,
                               const url::Origin& constructor_origin) = 0;

 protected:
  virtual ~SharedWorkerService() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SHARED_WORKER_SERVICE_H_
