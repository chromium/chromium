// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_PROTO_PROTO_UTILS_H_
#define MEDIA_CAST_LOGGING_PROTO_PROTO_UTILS_H_

#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/proto/raw_events.pb.h"

// Utility functions for cast logging protos.
namespace media {
namespace cast {

// Converts |event| to a corresponding value in |media::cast::proto::EventType|.
media::cast::proto::EventType ToProtoEventType(CastLoggingEvent event);

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_PROTO_PROTO_UTILS_H_
