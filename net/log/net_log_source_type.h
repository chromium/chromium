// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_NET_LOG_SOURCE_TYPE_H_
#define NET_LOG_NET_LOG_SOURCE_TYPE_H_

#include <stdint.h>

namespace net {

// The "source" identifies the entity that generated the log message.
enum class NetLogSourceType : uint32_t {
#define SOURCE_TYPE(label) label,
#include "net/log/net_log_source_type_list.h"
#undef SOURCE_TYPE
  COUNT
};

}  // namespace net

#endif  // NET_LOG_NET_LOG_SOURCE_TYPE_H_
