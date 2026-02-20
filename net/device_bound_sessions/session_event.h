// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_EVENT_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_EVENT_H_

#include <optional>
#include <variant>

#include "base/unguessable_token.h"
#include "net/base/net_export.h"
#include "net/base/schemeful_site.h"
#include "net/device_bound_sessions/challenge_result.h"
#include "net/device_bound_sessions/deletion_reason.h"
#include "net/device_bound_sessions/refresh_result.h"
#include "net/device_bound_sessions/session_display.h"
#include "net/device_bound_sessions/session_error.h"

namespace net::device_bound_sessions {

struct NET_EXPORT CreationEventDetails {
  CreationEventDetails();
  CreationEventDetails(const CreationEventDetails&);
  CreationEventDetails& operator=(const CreationEventDetails&);
  ~CreationEventDetails();
  CreationEventDetails(CreationEventDetails&& other);
  CreationEventDetails& operator=(CreationEventDetails&& other);

  SessionError::ErrorType fetch_error = SessionError::ErrorType::kSuccess;
  std::optional<SessionDisplay> new_session_display;
  std::optional<FailedRequest> failed_request;
};

struct NET_EXPORT RefreshEventDetails {
  RefreshEventDetails();
  RefreshEventDetails(const RefreshEventDetails&);
  RefreshEventDetails& operator=(const RefreshEventDetails&);
  ~RefreshEventDetails();
  RefreshEventDetails(RefreshEventDetails&& other);
  RefreshEventDetails& operator=(RefreshEventDetails&& other);

  RefreshResult refresh_result = RefreshResult::kRefreshed;
  // Proactive refresh refers to refreshes triggered before cookie expiry. A
  // fully proactive refresh means the refresh completed before any requests had
  // to be deferred.
  bool was_fully_proactive_refresh = false;
  std::optional<SessionError::ErrorType> fetch_error;
  std::optional<SessionDisplay> new_session_display;
  std::optional<FailedRequest> failed_request;
};

struct NET_EXPORT ChallengeEventDetails {
  ChallengeEventDetails();
  ChallengeEventDetails(ChallengeResult challenge_result,
                        std::string challenge);
  ChallengeEventDetails(const ChallengeEventDetails&);
  ChallengeEventDetails& operator=(const ChallengeEventDetails&);
  ~ChallengeEventDetails();
  ChallengeEventDetails(ChallengeEventDetails&& other);
  ChallengeEventDetails& operator=(ChallengeEventDetails&& other);

  ChallengeResult challenge_result = ChallengeResult::kSuccess;
  std::string challenge;
};

struct NET_EXPORT TerminationEventDetails {
  TerminationEventDetails();
  explicit TerminationEventDetails(DeletionReason deletion_reason);
  TerminationEventDetails(const TerminationEventDetails&);
  TerminationEventDetails& operator=(const TerminationEventDetails&);
  ~TerminationEventDetails();
  TerminationEventDetails(TerminationEventDetails&& other);
  TerminationEventDetails& operator=(TerminationEventDetails&& other);

  DeletionReason deletion_reason = DeletionReason::kExpired;
};

// LINT.IfChange(SessionEventTypeDetails)
using SessionEventTypeDetails = std::variant<CreationEventDetails,
                                             RefreshEventDetails,
                                             TerminationEventDetails,
                                             ChallengeEventDetails>;
// LINT.ThenChange(//services/network/public/cpp/device_bound_sessions_mojom_traits.h:SessionEventTypeDetails)

struct NET_EXPORT SessionEvent {
 public:
  static SessionEvent MakeCreationEvent(
      SchemefulSite site,
      std::optional<std::string> session_id,
      bool succeeded,
      SessionError fetch_error,
      std::optional<SessionDisplay> new_session_display);

  static SessionEvent MakeRefreshEvent(
      SchemefulSite site,
      const std::string& session_id,
      bool succeeded,
      RefreshResult refresh_result,
      std::optional<SessionError> fetch_error,
      std::optional<SessionDisplay> new_session_display,
      bool was_fully_proactive_refresh);

  static SessionEvent MakeChallengeEvent(SchemefulSite site,
                                         std::optional<std::string> session_id,
                                         bool succeeded,
                                         ChallengeResult challenge_result,
                                         const std::string& challenge);

  static SessionEvent MakeTerminationEvent(SchemefulSite site,
                                           const std::string& session_id,
                                           bool succeeded,
                                           DeletionReason deletion_reason);

  SessionEvent();
  ~SessionEvent();
  SessionEvent(const SessionEvent&);
  SessionEvent& operator=(const SessionEvent&);
  SessionEvent(SessionEvent&& other);
  SessionEvent& operator=(SessionEvent&& other);

  base::UnguessableToken event_id = base::UnguessableToken::Create();
  SchemefulSite site;
  std::optional<std::string> session_id;
  bool succeeded = false;
  SessionEventTypeDetails event_type_details;

 private:
  SessionEvent(SchemefulSite site,
               std::optional<std::string> session_id,
               bool succeeded);
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_EVENT_H_
