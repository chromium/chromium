// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BACKGROUND_SERVICE_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BACKGROUND_SERVICE_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/devtools_background_services.pb.h"
#include "content/browser/devtools/devtools_background_services_context_impl.h"
#include "content/browser/devtools/protocol/background_service.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"

namespace content {

class RenderFrameHostImpl;

namespace protocol {

class BackgroundServiceHandler
    : public DevToolsDomainHandler,
      public BackgroundService::Backend,
      public DevToolsBackgroundServicesContextImpl::EventObserver {
 public:
  BackgroundServiceHandler();
  ~BackgroundServiceHandler() override;

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  Response Disable() override;

  void StartObserving(
      const std::string& service,
      std::unique_ptr<StartObservingCallback> callback) override;
  Response StopObserving(const std::string& service) override;
  Response SetRecording(bool should_record,
                        const std::string& service) override;
  Response ClearEvents(const std::string& service) override;

 private:
  void DidGetLoggedEvents(
      devtools::proto::BackgroundService service,
      std::unique_ptr<StartObservingCallback> callback,
      std::vector<devtools::proto::BackgroundServiceEvent> events);

  void OnEventReceived(
      const devtools::proto::BackgroundServiceEvent& event) override;
  void OnRecordingStateChanged(
      bool should_record,
      devtools::proto::BackgroundService service) override;

  std::unique_ptr<BackgroundService::Frontend> frontend_;

  // Owned by the storage partition.
  DevToolsBackgroundServicesContextImpl* devtools_context_;

  base::flat_set<devtools::proto::BackgroundService> enabled_services_;

  base::WeakPtrFactory<BackgroundServiceHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BackgroundServiceHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BACKGROUND_SERVICE_HANDLER_H_
