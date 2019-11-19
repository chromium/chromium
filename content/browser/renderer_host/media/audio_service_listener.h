// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_SERVICE_LISTENER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_SERVICE_LISTENER_H_

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/optional.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"

namespace base {
class TickClock;
}

namespace content {

// Tracks the system's active audio service instance, if any exists.
class CONTENT_EXPORT AudioServiceListener
    : public service_manager::mojom::ServiceManagerListener {
 public:
  class CONTENT_EXPORT Metrics {
   public:
    // Matches histogram enum AudioServiceStartStatus, entries (except kMaxEnum)
    // must not be renumbered.
    enum class ServiceStartStatus {
      kAlreadyStarted = 0,
      kSuccess = 1,
      kFailure = 2,
      kMaxValue = kFailure,
    };

    explicit Metrics(const base::TickClock* clock);
    ~Metrics();

    void ServiceAlreadyRunning(service_manager::mojom::InstanceState state);
    void ServiceCreated();
    void ServiceFailedToStart();
    void ServiceStarted();
    void ServiceStopped();

   private:
    void LogServiceStartStatus(ServiceStartStatus status);

    const base::TickClock* clock_;
    base::TimeTicks initial_downtime_start_;
    base::TimeTicks created_;
    base::TimeTicks started_;
    base::TimeTicks stopped_;
    DISALLOW_COPY_AND_ASSIGN(Metrics);
  };

  explicit AudioServiceListener(
      std::unique_ptr<service_manager::Connector> connector);
  ~AudioServiceListener() override;

  base::ProcessId GetProcessId() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(AudioServiceListenerTest,
                           OnInitWithoutAudioService_ProcessIdNull);
  FRIEND_TEST_ALL_PREFIXES(AudioServiceListenerTest,
                           OnInitWithAudioService_ProcessIdNotNull);
  FRIEND_TEST_ALL_PREFIXES(AudioServiceListenerTest,
                           OnAudioServiceCreated_ProcessIdNotNull);
  FRIEND_TEST_ALL_PREFIXES(AudioServiceListenerTest,
                           StartService_LogStartStatus);

  // service_manager::mojom::ServiceManagerListener implementation.
  void OnInit(std::vector<service_manager::mojom::RunningServiceInfoPtr>
                  running_services) override;
  void OnServiceCreated(
      service_manager::mojom::RunningServiceInfoPtr service) override;
  void OnServiceStarted(const service_manager::Identity& identity,
                        uint32_t pid) override;
  void OnServicePIDReceived(const service_manager::Identity& identity,
                            uint32_t pid) override;
  void OnServiceFailedToStart(
      const service_manager::Identity& identity) override;
  void OnServiceStopped(const service_manager::Identity& identity) override;

  void MaybeSetLogFactory();

  mojo::Receiver<service_manager::mojom::ServiceManagerListener> receiver_{
      this};
  std::unique_ptr<service_manager::Connector> connector_;
  base::Optional<service_manager::Identity> current_instance_identity_;
  base::Optional<service_manager::mojom::InstanceState> current_instance_state_;
  base::ProcessId process_id_ = base::kNullProcessId;
  Metrics metrics_;
  bool log_factory_is_set_ = false;
  SEQUENCE_CHECKER(owning_sequence_);

  DISALLOW_COPY_AND_ASSIGN(AudioServiceListener);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_SERVICE_LISTENER_H_
