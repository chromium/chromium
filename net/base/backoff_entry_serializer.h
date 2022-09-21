// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_BACKOFF_ENTRY_SERIALIZER_H_
#define NET_BASE_BACKOFF_ENTRY_SERIALIZER_H_

#include <memory>

#include "base/time/time.h"
#include "base/values.h"
#include "net/base/backoff_entry.h"
#include "net/base/net_export.h"

namespace base {
class TickClock;
}

namespace net {

enum SerializationFormatVersion {
  kVersion1 = 1,
  kVersion2,
};

// Serialize or deserialize a BackoffEntry, so it can persist beyond the
// lifetime of the browser.
class NET_EXPORT BackoffEntrySerializer {
 public:
  BackoffEntrySerializer() = delete;
  BackoffEntrySerializer(const BackoffEntrySerializer&) = delete;
  BackoffEntrySerializer& operator=(const BackoffEntrySerializer&) = delete;

  // Serializes the release time and failure count into a List that can
  // later be passed to Deserialize to re-create the given BackoffEntry. It
  // always serializes using the latest format version. The Policy is not
  // serialized, instead callers must pass an identical Policy* when
  // deserializing. `time_now` should be `base::Time::Now()`, except for tests
  // that want to simulate time changes. The release time TimeTicks will be
  // converted to an absolute timestamp, thus the time will continue counting
  // down even whilst the device is powered off, and will be partially
  // vulnerable to changes in the system clock time.
  static base::Value::List SerializeToList(const BackoffEntry& entry,
                                           base::Time time_now);

  // Deserializes a `list` back to a BackoffEntry. It supports all
  // serialization format versions. `policy` MUST be the same Policy as the
  // serialized entry had. `clock` may be NULL. Both `policy` and `clock` (if
  // not NULL) must enclose lifetime of the returned BackoffEntry. `time_now`
  // should be `base::Time::Now()`, except for tests that want to simulate time
  // changes. The absolute timestamp that was serialized will be converted back
  // to TimeTicks as best as possible. Returns NULL if deserialization was
  // unsuccessful.
  static std::unique_ptr<BackoffEntry> DeserializeFromList(
      const base::Value::List& serialized,
      const BackoffEntry::Policy* policy,
      const base::TickClock* clock,
      base::Time time_now);
};

}  // namespace net

#endif  // NET_BASE_BACKOFF_ENTRY_SERIALIZER_H_
