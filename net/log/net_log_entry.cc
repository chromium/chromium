// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/net_log_entry.h"

#include "net/log/net_log.h"

namespace net {

NetLogEntry::NetLogEntry(NetLogEventType type,
                         NetLogSource source,
                         NetLogEventPhase phase,
                         base::TimeTicks time,
                         base::Value params)
    : type(type),
      source(source),
      phase(phase),
      time(time),
      params(std::move(params)) {}

NetLogEntry::~NetLogEntry() = default;

NetLogEntry::NetLogEntry(NetLogEntry&& entry) = default;
NetLogEntry& NetLogEntry::operator=(NetLogEntry&& entry) = default;

base::Value NetLogEntry::ToValue() const {
  base::Value entry_dict(base::Value::Type::DICTIONARY);

  entry_dict.SetStringKey("time", NetLog::TickCountToString(time));

  // Set the entry source.
  base::Value source_dict(base::Value::Type::DICTIONARY);
  source_dict.SetIntKey("id", source.id);
  source_dict.SetIntKey("type", static_cast<int>(source.type));
  source_dict.SetStringKey("start_time",
                           NetLog::TickCountToString(source.start_time));
  entry_dict.SetKey("source", std::move(source_dict));

  // Set the event info.
  entry_dict.SetIntKey("type", static_cast<int>(type));
  entry_dict.SetIntKey("phase", static_cast<int>(phase));

  // Set the event-specific parameters.
  if (!params.is_none())
    entry_dict.SetKey("params", params.Clone());

  return entry_dict;
}

NetLogEntry NetLogEntry::Clone() const {
  return NetLogEntry(type, source, phase, time, params.Clone());
}

bool NetLogEntry::HasParams() const {
  return !params.is_none();
}

}  // namespace net
