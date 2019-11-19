// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_NET_LOG_SOURCE_H_
#define NET_LOG_NET_LOG_SOURCE_H_

#include <stdint.h>

#include "net/base/net_export.h"
#include "net/log/net_log_source_type.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace net {

// Identifies the entity that generated this log. The |id| field should
// uniquely identify the source, and is used by log observers to infer
// message groupings. Can use NetLog::NextID() to create unique IDs.
struct NET_EXPORT NetLogSource {
  static const uint32_t kInvalidId;

  NetLogSource();
  NetLogSource(NetLogSourceType type, uint32_t id);
  bool IsValid() const;

  // Adds the source to a DictionaryValue containing event parameters,
  // using the name "source_dependency".
  void AddToEventParameters(base::Value* event_params) const;

  // Returns a dictionary with a single entry named "source_dependency" that
  // describes |this|.
  base::Value ToEventParameters() const;

  // Attempts to extract a NetLogSource from a set of event parameters.  Returns
  // true and writes the result to |source| on success.  Returns false and
  // makes |source| an invalid source on failure.
  // TODO(mmenke):  Long term, we want to remove this.
  static bool FromEventParameters(const base::Value* event_params,
                                  NetLogSource* source);

  NetLogSourceType type;
  uint32_t id;
};

}  // namespace net

#endif  // NET_LOG_NET_LOG_SOURCE_H_
