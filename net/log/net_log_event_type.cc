// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/net_log_event_type.h"

#include "base/notreached.h"

namespace net {

const char* NetLogEventTypeToString(NetLogEventType type) {
  switch (type) {
#define EVENT_TYPE(label)      \
  case NetLogEventType::label: \
    return #label;
#include "net/log/net_log_event_type_list.h"
#undef EVENT_TYPE
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

std::ostream& operator<<(std::ostream& os, NetLogEventType type) {
  return os << NetLogEventTypeToString(type);
}

}  // namespace net
