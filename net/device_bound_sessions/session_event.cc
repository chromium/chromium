// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "net/device_bound_sessions/session_event.h"

namespace net::device_bound_sessions {

SessionEvent::SessionEvent() = default;

SessionEvent::SessionEvent(SchemefulSite site,
                           std::optional<std::string> session_id,
                           bool succeeded)
    : site(std::move(site)),
      session_id(std::move(session_id)),
      succeeded(succeeded) {}

SessionEvent::~SessionEvent() = default;
SessionEvent::SessionEvent(const SessionEvent&) = default;
SessionEvent& SessionEvent::operator=(const SessionEvent&) = default;
SessionEvent::SessionEvent(SessionEvent&& other) = default;
SessionEvent& SessionEvent::operator=(SessionEvent&& other) = default;

CreationEventDetails::CreationEventDetails() = default;
CreationEventDetails::~CreationEventDetails() = default;
CreationEventDetails::CreationEventDetails(const CreationEventDetails&) =
    default;
CreationEventDetails& CreationEventDetails::operator=(
    const CreationEventDetails&) = default;
CreationEventDetails::CreationEventDetails(CreationEventDetails&& other) =
    default;
CreationEventDetails& CreationEventDetails::operator=(
    CreationEventDetails&& other) = default;

RefreshEventDetails::RefreshEventDetails() = default;
RefreshEventDetails::~RefreshEventDetails() = default;
RefreshEventDetails::RefreshEventDetails(const RefreshEventDetails&) = default;
RefreshEventDetails& RefreshEventDetails::operator=(
    const RefreshEventDetails&) = default;
RefreshEventDetails::RefreshEventDetails(RefreshEventDetails&& other) = default;
RefreshEventDetails& RefreshEventDetails::operator=(
    RefreshEventDetails&& other) = default;

ChallengeEventDetails::ChallengeEventDetails() = default;
ChallengeEventDetails::ChallengeEventDetails(ChallengeResult challenge_result,
                                             std::string challenge)
    : challenge_result(challenge_result), challenge(std::move(challenge)) {}
ChallengeEventDetails::~ChallengeEventDetails() = default;
ChallengeEventDetails::ChallengeEventDetails(const ChallengeEventDetails&) =
    default;
ChallengeEventDetails& ChallengeEventDetails::operator=(
    const ChallengeEventDetails&) = default;
ChallengeEventDetails::ChallengeEventDetails(ChallengeEventDetails&& other) =
    default;
ChallengeEventDetails& ChallengeEventDetails::operator=(
    ChallengeEventDetails&& other) = default;

TerminationEventDetails::TerminationEventDetails() = default;
TerminationEventDetails::TerminationEventDetails(DeletionReason deletion_reason)
    : deletion_reason(deletion_reason) {}
TerminationEventDetails::~TerminationEventDetails() = default;
TerminationEventDetails::TerminationEventDetails(
    const TerminationEventDetails&) = default;
TerminationEventDetails& TerminationEventDetails::operator=(
    const TerminationEventDetails&) = default;
TerminationEventDetails::TerminationEventDetails(
    TerminationEventDetails&& other) = default;
TerminationEventDetails& TerminationEventDetails::operator=(
    TerminationEventDetails&& other) = default;

// static
SessionEvent SessionEvent::MakeCreationEvent(
    SchemefulSite site,
    std::optional<std::string> session_id,
    bool succeeded,
    SessionError fetch_error,
    std::optional<SessionDisplay> new_session_display) {
  SessionEvent event(std::move(site), std::move(session_id), succeeded);
  auto& details = event.event_type_details.emplace<CreationEventDetails>();
  details.fetch_error = fetch_error.type;
  details.new_session_display = std::move(new_session_display);
  details.failed_request = std::move(fetch_error.failed_request);
  return event;
}

// static
SessionEvent SessionEvent::MakeRefreshEvent(
    SchemefulSite site,
    const std::string& session_id,
    bool succeeded,
    RefreshResult refresh_result,
    std::optional<SessionError> fetch_error,
    std::optional<SessionDisplay> new_session_display,
    bool was_fully_proactive_refresh) {
  SessionEvent event(std::move(site), session_id, succeeded);
  auto& details = event.event_type_details.emplace<RefreshEventDetails>();
  details.refresh_result = refresh_result;
  details.was_fully_proactive_refresh = was_fully_proactive_refresh;
  details.new_session_display = std::move(new_session_display);
  if (fetch_error) {
    details.fetch_error = fetch_error->type;
    details.failed_request = std::move(fetch_error->failed_request);
  }
  return event;
}

// static
SessionEvent SessionEvent::MakeChallengeEvent(
    SchemefulSite site,
    std::optional<std::string> session_id,
    bool succeeded,
    ChallengeResult challenge_result,
    const std::string& challenge) {
  SessionEvent event(std::move(site), std::move(session_id), succeeded);
  event.event_type_details.emplace<ChallengeEventDetails>(challenge_result,
                                                          challenge);
  return event;
}

// static
SessionEvent SessionEvent::MakeTerminationEvent(
    SchemefulSite site,
    const std::string& session_id,
    bool succeeded,
    DeletionReason deletion_reason) {
  SessionEvent event(std::move(site), session_id, succeeded);
  event.event_type_details.emplace<TerminationEventDetails>(deletion_reason);
  return event;
}

}  // namespace net::device_bound_sessions
