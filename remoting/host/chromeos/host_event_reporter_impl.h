// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_HOST_EVENT_REPORTER_IMPL_H_
#define REMOTING_HOST_CHROMEOS_HOST_EVENT_REPORTER_IMPL_H_

#include <memory>
#include <string>

#include "chrome/browser/policy/messaging_layer/proto/synced/crd_event.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "remoting/host/host_event_reporter.h"
#include "remoting/host/host_status_monitor.h"
#include "remoting/host/host_status_observer.h"

namespace remoting {

// Remoting host events reporter.
// Configured with the delegate for actual events delivery.
class HostEventReporterImpl : public HostEventReporter,
                              public HostStatusObserver {
 public:
  class Delegate {
   public:
    Delegate();
    Delegate(const Delegate& other) = delete;
    Delegate& operator=(const Delegate& other) = delete;
    virtual ~Delegate();

    // API to enqueue event. Must be implemented by subclass.
    virtual void EnqueueEvent(::ash::reporting::CRDRecord record) = 0;
  };

  HostEventReporterImpl(scoped_refptr<HostStatusMonitor> monitor,
                        std::unique_ptr<Delegate> delegate);
  HostEventReporterImpl(const HostEventReporterImpl& other) = delete;
  HostEventReporterImpl& operator=(const HostEventReporterImpl& other) = delete;
  ~HostEventReporterImpl() override;

  // HostStatusObserver implementation.  These methods will be called from the
  // network thread.
  void OnClientAccessDenied(const std::string& signaling_id) override;
  void OnClientAuthenticated(const std::string& signaling_id) override;
  void OnClientConnected(const std::string& signaling_id) override;
  void OnClientDisconnected(const std::string& signaling_id) override;
  void OnClientRouteChange(const std::string& signaling_id,
                           const std::string& channel_name,
                           const protocol::TransportRoute& route) override;
  void OnHostStarted(const std::string& owner_email) override;
  void OnHostShutdown() override;

 private:
  void ReportEvent(::ash::reporting::CRDRecord record);

  // Host user email.
  // Empty before the first call to ReportStart or after ReportStop.
  std::string host_user_;

  // Connection state (set piece by piece).
  std::string host_ip_;
  std::string client_ip_;
  std::string session_id_;

  // Event enqueueing delegate.
  std::unique_ptr<Delegate> delegate_;

  // Status monitor to observe.
  scoped_refptr<HostStatusMonitor> monitor_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_HOST_EVENT_REPORTER_IMPL_H_
