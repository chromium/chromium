// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_NET_LOG_SOURCE_H_
#define NET_LOG_NET_LOG_SOURCE_H_

#include <stdint.h>

#include "base/time/time.h"
#include "base/values.h"
#include "net/base/net_export.h"
#include "net/log/net_log_source_type.h"

namespace net {

// Identifies the entity that generated this log. The |id| field should
// uniquely identify the source, and is used by log observers to infer
// message groupings. Can use NetLog::NextID() to create unique IDs.
struct NET_EXPORT NetLogSource {
  static const uint32_t kInvalidId;

  NetLogSource();
  NetLogSource(NetLogSourceType type, uint32_t id);
  NetLogSource(NetLogSourceType type, uint32_t id, base::TimeTicks start_time);

  bool operator==(const NetLogSource& rhs) const;

  bool IsValid() const;

  // Adds the source to a dictionary containing event parameters,
  // using the name "source_dependency".
  void AddToEventParameters(base::Value::Dict& event_params) const;

  // Returns a dictionary with a single entry named "source_dependency" that
  // describes |this|.
  base::Value::Dict ToEventParameters() const;

  NetLogSourceType type;
  uint32_t id;
  base::TimeTicks start_time;
};

}  // namespace net

#endif  // NET_LOG_NET_LOG_SOURCE_H_
