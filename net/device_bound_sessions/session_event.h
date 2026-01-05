// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_EVENT_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_EVENT_H_

#include <optional>

#include "base/unguessable_token.h"
#include "net/base/net_export.h"
#include "net/base/schemeful_site.h"

namespace net::device_bound_sessions {

struct NET_EXPORT SessionEvent {
 public:
  enum class EventType {};

  ~SessionEvent();
  SessionEvent(const SessionEvent&);
  SessionEvent& operator=(const SessionEvent&);
  SessionEvent(SessionEvent&& other);
  SessionEvent& operator=(SessionEvent&& other);

  base::UnguessableToken event_id = base::UnguessableToken::Create();
  SchemefulSite site;
  std::optional<std::string> session_id;
  // TODO(crbug.com/471017387): Add a default once EventType has any values.
  EventType event_type;
  bool succeeded = false;

 private:
  SessionEvent();
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_EVENT_H_
