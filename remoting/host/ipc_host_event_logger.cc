// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_host_event_logger.h"

#include "base/check_op.h"
#include "ipc/ipc_sender.h"
#include "net/base/ip_endpoint.h"
#include "remoting/host/host_status_monitor.h"
#include "remoting/protocol/transport.h"

namespace remoting {

IpcHostEventLogger::IpcHostEventLogger(
    scoped_refptr<HostStatusMonitor> monitor,
    mojo::AssociatedRemote<mojom::HostStatusObserver> remote)
    : host_status_observer_(std::move(remote)), monitor_(monitor) {
  monitor_->AddStatusObserver(this);
}

IpcHostEventLogger::~IpcHostEventLogger() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  monitor_->RemoveStatusObserver(this);
}

void IpcHostEventLogger::OnClientAccessDenied(const std::string& signaling_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_status_observer_->OnClientAccessDenied(signaling_id);
}

void IpcHostEventLogger::OnClientAuthenticated(
    const std::string& signaling_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_status_observer_->OnClientAuthenticated(signaling_id);
}

void IpcHostEventLogger::OnClientConnected(const std::string& signaling_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_status_observer_->OnClientConnected(signaling_id);
}

void IpcHostEventLogger::OnClientDisconnected(const std::string& signaling_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_status_observer_->OnClientDisconnected(signaling_id);
}

void IpcHostEventLogger::OnClientRouteChange(
    const std::string& signaling_id,
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_status_observer_->OnClientRouteChange(signaling_id, channel_name, route);
}

void IpcHostEventLogger::OnHostShutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_status_observer_->OnHostShutdown();
}

void IpcHostEventLogger::OnHostStarted(const std::string& owner_email) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_status_observer_->OnHostStarted(owner_email);
}

}  // namespace remoting
