// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "net/device_bound_sessions/session_event.h"

namespace net::device_bound_sessions {

SessionEvent::SessionEvent(EventType event_type,
                           SchemefulSite site,
                           std::optional<std::string> session_id,
                           bool succeeded)
    : site(std::move(site)),
      session_id(std::move(session_id)),
      event_type(event_type),
      succeeded(succeeded) {}

SessionEvent::~SessionEvent() = default;
SessionEvent::SessionEvent(const SessionEvent&) = default;
SessionEvent& SessionEvent::operator=(const SessionEvent&) = default;
SessionEvent::SessionEvent(SessionEvent&& other) = default;
SessionEvent& SessionEvent::operator=(SessionEvent&& other) = default;

// static
SessionEvent SessionEvent::MakeCreationEvent(
    SchemefulSite site,
    std::optional<std::string> session_id,
    bool succeeded,
    SessionError::ErrorType fetch_error,
    std::optional<SessionDisplay> new_session_display) {
  SessionEvent event(EventType::kCreation, std::move(site),
                     std::move(session_id), succeeded);
  event.fetch_error = fetch_error;
  event.new_session_display = std::move(new_session_display);
  return event;
}

// static
SessionEvent SessionEvent::MakeRefreshEvent(
    SchemefulSite site,
    const std::string& session_id,
    bool succeeded,
    RefreshResult refresh_result,
    std::optional<SessionError::ErrorType> fetch_error,
    std::optional<SessionDisplay> new_session_display,
    bool was_fully_proactive_refresh) {
  SessionEvent event(EventType::kRefresh, std::move(site), session_id,
                     succeeded);
  event.refresh_result = refresh_result;
  event.fetch_error = fetch_error;
  event.new_session_display = std::move(new_session_display);
  event.was_fully_proactive_refresh = was_fully_proactive_refresh;
  return event;
}

}  // namespace net::device_bound_sessions
