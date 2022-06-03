// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_LOG_RECORD_H_
#define MEDIA_BASE_MEDIA_LOG_RECORD_H_

#include <stdint.h>
#include <memory>

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
    std::unique_ptr<base::DictionaryValue> event_copy(event.params.DeepCopy());
    params.Swap(event_copy.get());
    time = event.time;
    return *this;
  }

  enum class Type {
    // See media/base/media_log_message_levels.h for info.
    kMessage,

    // See media/base/media_log_properties.h for info.
    kMediaPropertyChange,

    // See media/base/media_log_events.h for info.
    kMediaEventTriggered,

    // TODO(tmathmeyer) use media::Status eventually instead of PipelineStatus
    kMediaStatus,

    kMaxValue = kMediaStatus,
  };

  int32_t id;
  Type type;
  base::DictionaryValue params;
  base::TimeTicks time;
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_LOG_RECORD_H_
