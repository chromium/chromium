// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_CONNECTION_EVENT_TRACKER_H_
#define GOOGLE_APIS_GCM_ENGINE_CONNECTION_EVENT_TRACKER_H_

#include <stdint.h>

#include "base/containers/circular_deque.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "net/base/net_errors.h"

namespace gcm {

class GCM_EXPORT ConnectionEventTracker {
 public:
  // TODO(harkness): Pass in the storage information.
  ConnectionEventTracker();

  ConnectionEventTracker(const ConnectionEventTracker&) = delete;
  ConnectionEventTracker& operator=(const ConnectionEventTracker&) = delete;

  ~ConnectionEventTracker();

  // Returns a boolean indicating whether an attempt is currently in progress.
  bool IsEventInProgress() const;

  // Start recording a new connection attempt. This should never be called if
  // a connection attempt is already ongoing.
  void StartConnectionAttempt();

  // Ends the record for a connection attempt and moves it to the completed
  // connections list.
  void EndConnectionAttempt();

  // Record that the existing connection attempt has succeeded. Note that this
  // doesn't mean the connection is necessarily valid. It could still fail with
  // an authentication error.
  void ConnectionAttemptSucceeded();

  // Records that the connection succeeded but then failed to login.
  void ConnectionLoginFailed();

  // Record that the existing connection attempt has failed.
  void ConnectionAttemptFailed(int error);

  // Write any previous connection information to the |*login_request|.
  void WriteToLoginRequest(mcs_proto::LoginRequest* login_request);

 private:
  // Storage for all the events which have completed.
  base::circular_deque<mcs_proto::ClientEvent> completed_events_;

  // Current connection attempt.
  mcs_proto::ClientEvent current_event_;

  // Number of events which were discarded due to exceeding the total number of
  // events collected. This is sent to GCM to represent those events.
  uint32_t number_discarded_events_ = 0;
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_CONNECTION_EVENT_TRACKER_H_
