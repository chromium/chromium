// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_DEDICATED_WORKER_SERVICE_IMPL_H_
#define CONTENT_BROWSER_WORKER_HOST_DEDICATED_WORKER_SERVICE_IMPL_H_

#include "base/observer_list.h"
#include "content/public/browser/dedicated_worker_service.h"
#include "url/gurl.h"

namespace content {

class DedicatedWorkerHost;

class CONTENT_EXPORT DedicatedWorkerServiceImpl
    : public DedicatedWorkerService {
 public:
  DedicatedWorkerServiceImpl();
  ~DedicatedWorkerServiceImpl() override;

  DedicatedWorkerServiceImpl(const DedicatedWorkerServiceImpl& other) = delete;

  // DedicatedWorkerService:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void EnumerateDedicatedWorkers(Observer* observer) override;

  // Notifies all observers about a new worker.
  void NotifyWorkerCreated(const blink::DedicatedWorkerToken& worker_token,
                           int worker_process_id,
                           GlobalFrameRoutingId ancestor_render_frame_host_id,
                           DedicatedWorkerHost* host);

  // Notifies all observers about a worker being destroyed.
  void NotifyBeforeWorkerDestroyed(
      const blink::DedicatedWorkerToken& worker_token,
      GlobalFrameRoutingId ancestor_render_frame_host_id);

  // Notifies all observers that a worker's final response URL was determined.
  void NotifyWorkerFinalResponseURLDetermined(
      const blink::DedicatedWorkerToken& worker_token,
      const GURL& url);

  // Returns true if a worker with the given token has already been registered
  // with the service. This allows for malformed messages with duplicated
  // tokens to be detected, and the offending renderer to be shutdown.
  bool HasToken(const blink::DedicatedWorkerToken& worker_token) const;

  // Returns the DedicatedWorkerHost associated with this token. Clients should
  // not hold on to the pointer, as it may become invalid when the worker exits.
  DedicatedWorkerHost* GetDedicatedWorkerHostFromToken(
      const blink::DedicatedWorkerToken& worker_token) const;

 private:
  base::ObserverList<Observer> observers_;

  // TODO(chromium:1145158): Remove this struct.
  struct DedicatedWorkerInfo {
    DedicatedWorkerInfo(int worker_process_id,
                        GlobalFrameRoutingId ancestor_render_frame_host_id,
                        DedicatedWorkerHost* dedicated_worker_host);
    ~DedicatedWorkerInfo();

    DedicatedWorkerInfo(const DedicatedWorkerInfo& info);
    DedicatedWorkerInfo& operator=(const DedicatedWorkerInfo& info);

    int worker_process_id;
    GlobalFrameRoutingId ancestor_render_frame_host_id;
    base::Optional<GURL> final_response_url;
    // This rawptr is valid while the corresponding DedicatedWorkerInfo is kept
    // by DedicatedWorkerServiceImpl.
    DedicatedWorkerHost* dedicated_worker_host;
  };
  base::flat_map<blink::DedicatedWorkerToken, DedicatedWorkerInfo>
      dedicated_worker_infos_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_DEDICATED_WORKER_SERVICE_IMPL_H_
