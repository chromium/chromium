// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_MONITORING_GCM_STATS_RECORDER_H_
#define GOOGLE_APIS_GCM_MONITORING_GCM_STATS_RECORDER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "google_apis/gcm/base/gcm_export.h"
#include "google_apis/gcm/engine/connection_factory.h"
#include "google_apis/gcm/engine/mcs_client.h"
#include "google_apis/gcm/engine/registration_request.h"
#include "google_apis/gcm/engine/unregistration_request.h"

namespace gcm {

// Defines the interface to record GCM internal stats and activities for
// debugging purpose.
class GCM_EXPORT GCMStatsRecorder {
 public:
  // Type of a received message
  enum ReceivedMessageType {
    // Data message.
    DATA_MESSAGE,
    // Message that indicates some messages have been deleted on the server.
    DELETED_MESSAGES,
  };

  // A delegate interface that allows the GCMStatsRecorderImpl instance to
  // interact with its container.
  class Delegate {
   public:
    // Called when the GCMStatsRecorderImpl is recording activities and a new
    // activity has just been recorded.
    virtual void OnActivityRecorded() = 0;
  };

  GCMStatsRecorder() {}
  virtual ~GCMStatsRecorder() {}

  // Records that a check-in has been initiated.
  virtual void RecordCheckinInitiated(uint64_t android_id) = 0;

  // Records that a check-in has been delayed due to backoff.
  virtual void RecordCheckinDelayedDueToBackoff(int64_t delay_msec) = 0;

  // Records that a check-in request has succeeded.
  virtual void RecordCheckinSuccess() = 0;

  // Records that a check-in request has failed. If a retry will be tempted then
  // will_retry should be true.
  virtual void RecordCheckinFailure(const std::string& status,
                                    bool will_retry) = 0;

  // Records that a connection to MCS has been initiated.
  virtual void RecordConnectionInitiated(const std::string& host) = 0;

  // Records that a connection has been delayed due to backoff.
  virtual void RecordConnectionDelayedDueToBackoff(int64_t delay_msec) = 0;

  // Records that connection has been successfully established.
  virtual void RecordConnectionSuccess() = 0;

  // Records that connection has failed with a network error code.
  virtual void RecordConnectionFailure(int network_error) = 0;

  // Records that connection reset has been signaled.
  virtual void RecordConnectionResetSignaled(
      ConnectionFactory::ConnectionResetReason reason) = 0;

  // Records that a registration request has been sent. This could be initiated
  // directly from API, or from retry logic.
  virtual void RecordRegistrationSent(const std::string& app_id,
                                      const std::string& source) = 0;

  // Records that a registration response has been received from server.
  virtual void RecordRegistrationResponse(
      const std::string& app_id,
      const std::string& source,
      RegistrationRequest::Status status) = 0;

  // Records that a registration retry has been requested and delayed due to
  // backoff logic.
  virtual void RecordRegistrationRetryDelayed(const std::string& app_id,
                                              const std::string& source,
                                              int64_t delay_msec,
                                              int retries_left) = 0;

  // Records that an unregistration request has been sent. This could be
  // initiated directly from API, or from retry logic.
  virtual void RecordUnregistrationSent(const std::string& app_id,
                                        const std::string& source) = 0;

  // Records that an unregistration response has been received from server.
  virtual void RecordUnregistrationResponse(
      const std::string& app_id,
      const std::string& source,
      UnregistrationRequest::Status status) = 0;

  // Records that an unregistration retry has been requested and delayed due to
  // backoff logic.
  virtual void RecordUnregistrationRetryDelayed(const std::string& app_id,
                                                const std::string& source,
                                                int64_t delay_msec,
                                                int retries_left) = 0;

  // Records that a data message has been received. If it indicates that one or
  // more messages were dropped on the server, message_type should be
  // DELETED_MESSAGES.
  virtual void RecordDataMessageReceived(const std::string& app_id,
                                         const std::string& from,
                                         int message_byte_size,
                                         ReceivedMessageType message_type) = 0;

  // Records that an outgoing data message was sent over the wire.
  virtual void RecordDataSentToWire(const std::string& app_id,
                                    const std::string& receiver_id,
                                    const std::string& message_id,
                                    int queued) = 0;
  // Records that the MCS client sent a 'send status' notification to callback.
  virtual void RecordNotifySendStatus(const std::string& app_id,
                                      const std::string& receiver_id,
                                      const std::string& message_id,
                                      MCSClient::MessageSendStatus status,
                                      int byte_size,
                                      int ttl) = 0;
  // Records that a 'send error' message was received.
  virtual void RecordIncomingSendError(const std::string& app_id,
                                       const std::string& receiver_id,
                                       const std::string& message_id) = 0;
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_MONITORING_GCM_STATS_RECORDER_H_
