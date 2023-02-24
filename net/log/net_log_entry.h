// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_NET_LOG_ENTRY_H_
#define NET_LOG_NET_LOG_ENTRY_H_

#include "base/time/time.h"
#include "base/values.h"
#include "net/base/net_export.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"

namespace net {

// Represents an event that was sent to a NetLog observer, including the
// materialized parameters.
struct NET_EXPORT NetLogEntry {
 public:
  NetLogEntry(NetLogEventType type,
              NetLogSource source,
              NetLogEventPhase phase,
              base::TimeTicks time,
              base::Value::Dict params);

  ~NetLogEntry();

  // Moveable.
  NetLogEntry(NetLogEntry&& entry);
  NetLogEntry& operator=(NetLogEntry&& entry);

  // Serializes the specified event to a Dict.
  base::Value::Dict ToDict() const;

  // NetLogEntry is not copy constructible, however copying is useful for
  // unittests.
  NetLogEntry Clone() const;

  // Returns true if the entry has value for |params|.
  bool HasParams() const;

  NetLogEventType type;
  NetLogSource source;
  NetLogEventPhase phase;
  base::TimeTicks time;
  base::Value::Dict params;
};

}  // namespace net

#endif  // NET_LOG_NET_LOG_ENTRY_H_
