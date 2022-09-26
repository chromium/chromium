// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_LOG_RECORD_H_
#define MEDIA_BASE_MEDIA_LOG_RECORD_H_

#include <stdint.h>

#include "base/time/time.h"
#include "base/values.h"

namespace media {

// TODO(tmathmeyer) refactor this so that we aren't packing type-erased data
// with different meanings into "params".
struct MediaLogRecord {
  MediaLogRecord() {}

  MediaLogRecord(const MediaLogRecord& event) { *this = event; }

  MediaLogRecord& operator=(const MediaLogRecord& event) {
    id = event.id;
    type = event.type;
    params = event.params.Clone();
    time = event.time;
    return *this;
  }

  bool operator==(const MediaLogRecord& other) const {
    return id == other.id && type == other.type && params == other.params &&
           time == other.time;
  }
  bool operator!=(const MediaLogRecord& other) const {
    return !(*this == other);
  }

  enum class Type {
    // See media/base/media_log_message_levels.h for info.
    kMessage,

    // See media/base/media_log_properties.h for info.
    kMediaPropertyChange,

    // See media/base/media_log_events.h for info.
    kMediaEventTriggered,

    // Represents the contents some TypedStatus<T>
    kMediaStatus,

    kMaxValue = kMediaStatus,
  };

  int32_t id;
  Type type;
  base::Value::Dict params;
  base::TimeTicks time;
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_LOG_RECORD_H_
