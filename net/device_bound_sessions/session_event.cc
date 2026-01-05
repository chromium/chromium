// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "net/device_bound_sessions/session_event.h"

namespace net::device_bound_sessions {

SessionEvent::~SessionEvent() = default;
SessionEvent::SessionEvent(const SessionEvent&) = default;
SessionEvent& SessionEvent::operator=(const SessionEvent&) = default;
SessionEvent::SessionEvent(SessionEvent&& other) = default;
SessionEvent& SessionEvent::operator=(SessionEvent&& other) = default;

}  // namespace net::device_bound_sessions
