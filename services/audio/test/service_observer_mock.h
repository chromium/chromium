// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_TEST_SERVICE_OBSERVER_MOCK_H_
#define SERVICES_AUDIO_TEST_SERVICE_OBSERVER_MOCK_H_

#include <string>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace audio {

// Mock listener tracking lifetime events for a specified service.
class ServiceObserverMock
    : public service_manager::mojom::ServiceManagerListener {
 public:
  ServiceObserverMock(
      const std::string& service_name,
      mojo::PendingReceiver<service_manager::mojom::ServiceManagerListener>
          receiver);
  ~ServiceObserverMock() override;

  MOCK_METHOD0(Initialized, void(void));
  MOCK_METHOD0(ServiceStarted, void(void));
  MOCK_METHOD0(ServiceStopped, void(void));

  // mojom::ServiceManagerListener implementation.
  void OnInit(std::vector<service_manager::mojom::RunningServiceInfoPtr>
                  instances) override;
  void OnServiceCreated(
      service_manager::mojom::RunningServiceInfoPtr instance) override {}
  void OnServiceStarted(const service_manager::Identity& identity,
                        uint32_t pid) override;
  void OnServiceFailedToStart(
      const service_manager::Identity& identity) override {}
  void OnServicePIDReceived(const service_manager::Identity& identity,
                            uint32_t pid) override {}
  void OnServiceStopped(const service_manager::Identity& identity) override;

 private:
  const std::string service_name_;
  mojo::Receiver<service_manager::mojom::ServiceManagerListener> receiver_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_TEST_SERVICE_OBSERVER_MOCK_H_
