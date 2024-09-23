// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/net_log_entry.h"

#include <utility>

#include "base/time/time.h"
#include "base/values.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"

namespace net {

NetLogEntry::NetLogEntry(NetLogEventType type,
                         NetLogSource source,
                         NetLogEventPhase phase,
                         base::TimeTicks time,
                         base::Value::Dict params)
    : type(type),
      source(source),
      phase(phase),
      time(time),
      params(std::move(params)) {}

NetLogEntry::~NetLogEntry() = default;

NetLogEntry::NetLogEntry(NetLogEntry&& entry) = default;
NetLogEntry& NetLogEntry::operator=(NetLogEntry&& entry) = default;

base::Value::Dict NetLogEntry::ToDict() const {
  base::Value::Dict entry_dict;

  entry_dict.Set("time", NetLog::TickCountToString(time));

  // Set the entry source.
  base::Value::Dict source_dict;
  source_dict.Set("id", static_cast<int>(source.id));
  source_dict.Set("type", static_cast<int>(source.type));
  source_dict.Set("start_time", NetLog::TickCountToString(source.start_time));
  entry_dict.Set("source", std::move(source_dict));

  // Set the event info.
  entry_dict.Set("type", static_cast<int>(type));
  entry_dict.Set("phase", static_cast<int>(phase));

  // Set the event-specific parameters.
  if (!params.empty()) {
    entry_dict.Set("params", params.Clone());
  }

  return entry_dict;
}

NetLogEntry NetLogEntry::Clone() const {
  return NetLogEntry(type, source, phase, time, params.Clone());
}

bool NetLogEntry::HasParams() const {
  return !params.empty();
}

}  // namespace net
