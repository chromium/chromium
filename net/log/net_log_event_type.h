// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_NET_LOG_EVENT_TYPE_H_
#define NET_LOG_NET_LOG_EVENT_TYPE_H_

#include <ostream>

#include "net/base/net_export.h"

namespace net {

enum class NetLogEventType {
#define EVENT_TYPE(label) label,
#include "net/log/net_log_event_type_list.h"
#undef EVENT_TYPE
  COUNT
};

// Returns a C-String symbolic name for |type|.
NET_EXPORT const char* NetLogEventTypeToString(NetLogEventType type);

// For convenience in tests.
NET_EXPORT std::ostream& operator<<(std::ostream& os, NetLogEventType type);

// The 'phase' of an event trace (whether it marks the beginning or end
// of an event.).
enum class NetLogEventPhase {
  NONE,
  BEGIN,
  END,
};

}  // namespace net

#endif  // NET_LOG_NET_LOG_EVENT_TYPE_H_
