// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/net_log_source.h"

#include <memory>
#include <utility>

#include "base/values.h"

namespace net {

// LoadTimingInfo requires this be 0.
const uint32_t NetLogSource::kInvalidId = 0;

NetLogSource::NetLogSource()
    : NetLogSource(NetLogSourceType::NONE, kInvalidId) {}

NetLogSource::NetLogSource(NetLogSourceType type, uint32_t id)
    : NetLogSource(type, id, base::TimeTicks::Now()) {}

NetLogSource::NetLogSource(NetLogSourceType type,
                           uint32_t id,
                           base::TimeTicks start_time)
    : type(type), id(id), start_time(start_time) {}

bool NetLogSource::operator==(const NetLogSource& rhs) const {
  return type == rhs.type && id == rhs.id && start_time == rhs.start_time;
}

bool NetLogSource::IsValid() const {
  return id != kInvalidId;
}

void NetLogSource::AddToEventParameters(base::Value::Dict& event_params) const {
  base::Value::Dict dict;
  dict.Set("type", static_cast<int>(type));
  dict.Set("id", static_cast<int>(id));
  event_params.Set("source_dependency", std::move(dict));
}

base::Value::Dict NetLogSource::ToEventParameters() const {
  if (!IsValid())
    return base::Value::Dict();
  base::Value::Dict event_params;
  AddToEventParameters(event_params);
  return event_params;
}

}  // namespace net
