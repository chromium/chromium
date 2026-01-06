// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_EVENT_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_EVENT_H_

#include <optional>

#include "base/unguessable_token.h"
#include "net/base/net_export.h"
#include "net/base/schemeful_site.h"
#include "net/device_bound_sessions/refresh_result.h"
#include "net/device_bound_sessions/session_display.h"
#include "net/device_bound_sessions/session_error.h"

namespace net::device_bound_sessions {

struct NET_EXPORT SessionEvent {
 public:
  enum class EventType {
    kCreation,
    kRefresh,
  };

  static SessionEvent MakeCreationEvent(
      SchemefulSite site,
      std::optional<std::string> session_id,
      bool succeeded,
      SessionError::ErrorType fetch_error,
      std::optional<SessionDisplay> new_session_display);

  static SessionEvent MakeRefreshEvent(
      SchemefulSite site,
      const std::string& session_id,
      bool succeeded,
      RefreshResult refresh_result,
      std::optional<SessionError::ErrorType> fetch_error,
      std::optional<SessionDisplay> new_session_display,
      bool was_fully_proactive_refresh);

  ~SessionEvent();
  SessionEvent(const SessionEvent&);
  SessionEvent& operator=(const SessionEvent&);
  SessionEvent(SessionEvent&& other);
  SessionEvent& operator=(SessionEvent&& other);

  base::UnguessableToken event_id = base::UnguessableToken::Create();
  SchemefulSite site;
  std::optional<std::string> session_id;
  EventType event_type = EventType::kCreation;
  bool succeeded = false;

  // TODO(crbug.com/471021582): Add additional fields.
  std::optional<SessionError::ErrorType> fetch_error;
  std::optional<SessionDisplay> new_session_display;
  std::optional<RefreshResult> refresh_result;
  // Proactive refresh refers to refreshes triggered before cookie expiry. A
  // fully proactive refresh means the refresh completed before any requests had
  // to be deferred.
  std::optional<bool> was_fully_proactive_refresh;

 private:
  SessionEvent(EventType event_type,
               SchemefulSite site,
               std::optional<std::string> session_id,
               bool succeeded);
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_EVENT_H_
