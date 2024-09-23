// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/connection_event_tracker.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "net/base/network_change_notifier.h"

namespace {

// The maxiumum number of events which are stored before deleting old ones.
// This mirrors the behaviour of the GMS Core connection tracking.
constexpr size_t kMaxClientEvents = 30;

}  // namespace

namespace gcm {

ConnectionEventTracker::ConnectionEventTracker() = default;

ConnectionEventTracker::~ConnectionEventTracker() = default;

bool ConnectionEventTracker::IsEventInProgress() const {
  return current_event_.has_time_connection_started_ms();
}

void ConnectionEventTracker::StartConnectionAttempt() {
  // TODO(harkness): Can we dcheck here that there is not an in progress
  // connection?
  current_event_.set_time_connection_started_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  // The connection type is passed to the server and stored there, so the
  // values should remain consistent.
  current_event_.set_network_type(
      static_cast<int>(net::NetworkChangeNotifier::GetConnectionType()));
}

void ConnectionEventTracker::EndConnectionAttempt() {
  DCHECK(IsEventInProgress());

  if (completed_events_.size() == kMaxClientEvents) {
    // Don't let the completed events grow beyond the max.
    completed_events_.pop_front();
    number_discarded_events_++;
  }

  // Current event is finished, so add it to our list of completed events.
  current_event_.set_time_connection_ended_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  completed_events_.push_back(current_event_);
  current_event_.Clear();
}

void ConnectionEventTracker::ConnectionAttemptSucceeded() {
  // Record the successful connection so information about it can be sent in the
  // next login request. If there is a login failure, this will need to be
  // updated to a failed connection.
  current_event_.set_type(mcs_proto::ClientEvent::SUCCESSFUL_CONNECTION);
  current_event_.set_time_connection_established_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());

  // A completed connection means that the old client event data has now been
  // sent to GCM. Delete old data.
  completed_events_.clear();
  number_discarded_events_ = 0;
}

void ConnectionEventTracker::ConnectionLoginFailed() {
  // A login failure would have originally been marked as a successful
  // connection, so now that it failed, that needs to be updated.
  DCHECK_EQ(current_event_.type(),
            mcs_proto::ClientEvent::SUCCESSFUL_CONNECTION);

  current_event_.set_type(mcs_proto::ClientEvent::FAILED_CONNECTION);
  current_event_.clear_time_connection_established_ms();
  current_event_.set_error_code(net::ERR_CONNECTION_RESET);
}

void ConnectionEventTracker::ConnectionAttemptFailed(int error) {
  DCHECK_NE(error, net::OK);

  current_event_.set_type(mcs_proto::ClientEvent::FAILED_CONNECTION);
  current_event_.set_error_code(error);
}

void ConnectionEventTracker::WriteToLoginRequest(
    mcs_proto::LoginRequest* request) {
  DCHECK(request);

  // Add an event to represented the discarded events if needed.
  if (number_discarded_events_ > 0) {
    mcs_proto::ClientEvent* event = request->add_client_event();
    event->set_type(mcs_proto::ClientEvent::DISCARDED_EVENTS);
    event->set_number_discarded_events(number_discarded_events_);
  }

  for (const mcs_proto::ClientEvent& event : completed_events_)
    request->add_client_event()->CopyFrom(event);
}

}  // namespace gcm
