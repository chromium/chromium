// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/host_event_reporter_impl.h"

#include <memory>

#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/crd_event.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "remoting/protocol/transport.h"

namespace remoting {

HostEventReporterImpl::Delegate::Delegate() = default;
HostEventReporterImpl::Delegate::~Delegate() = default;

HostEventReporterImpl::HostEventReporterImpl(
    scoped_refptr<HostStatusMonitor> monitor,
    std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)), monitor_(monitor) {
  monitor_->AddStatusObserver(this);
}

HostEventReporterImpl::~HostEventReporterImpl() {
  monitor_->RemoveStatusObserver(this);
}

void HostEventReporterImpl::OnClientAccessDenied(
    const std::string& signaling_id) {}

void HostEventReporterImpl::OnClientAuthenticated(
    const std::string& signaling_id) {}

void HostEventReporterImpl::OnClientConnected(const std::string& signaling_id) {
}

void HostEventReporterImpl::OnClientDisconnected(
    const std::string& signaling_id) {
  ::ash::reporting::CRDRecord record;
  ::ash::reporting::CRDClientDisconnected* const state =
      record.mutable_disconnected();
  state->set_host_ip(host_ip_);
  state->set_client_ip(client_ip_);
  state->set_session_id(signaling_id);
  ReportEvent(std::move(record));
  host_ip_.clear();
  client_ip_.clear();
  session_id_.clear();
}

void HostEventReporterImpl::OnClientRouteChange(
    const std::string& signaling_id,
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  client_ip_ = route.remote_address.address().IsValid()
                   ? route.remote_address.ToString().c_str()
                   : "unknown";
  host_ip_ = route.local_address.address().IsValid()
                 ? route.local_address.ToString().c_str()
                 : "unknown";
  ::ash::reporting::ConnectionType connection_type;
  switch (route.type) {
    case ::remoting::protocol::TransportRoute::DIRECT:
      connection_type = ::ash::reporting::ConnectionType::CRD_CONNECTION_DIRECT;
      break;
    case ::remoting::protocol::TransportRoute::RELAY:
      connection_type = ::ash::reporting::ConnectionType::CRD_CONNECTION_RELAY;
      break;
    case ::remoting::protocol::TransportRoute::STUN:
      connection_type = ::ash::reporting::ConnectionType::CRD_CONNECTION_STUN;
      break;
    default:
      connection_type =
          ::ash::reporting::ConnectionType::CRD_CONNECTION_UNKNOWN;
  }
  ::ash::reporting::CRDRecord record;
  ::ash::reporting::CRDClientConnected* const state =
      record.mutable_connected();
  state->set_host_ip(host_ip_);
  state->set_client_ip(client_ip_);
  state->set_session_id(signaling_id);
  state->set_connection_type(connection_type);
  ReportEvent(std::move(record));
}

void HostEventReporterImpl::OnHostStarted(const std::string& owner_email) {
  host_user_ = owner_email;
  ::ash::reporting::CRDRecord record;
  record.mutable_started();
  ReportEvent(std::move(record));
}

void HostEventReporterImpl::OnHostShutdown() {
  ::ash::reporting::CRDRecord record;
  record.mutable_ended();
  ReportEvent(std::move(record));
  host_user_.clear();
}

void HostEventReporterImpl::ReportEvent(::ash::reporting::CRDRecord record) {
  DCHECK(!host_user_.empty());
  record.set_event_timestamp_sec(base::Time::Now().ToTimeT());
  record.mutable_host_user()->set_user_email(host_user_);
  delegate_->EnqueueEvent(std::move(record));
}

}  // namespace remoting
