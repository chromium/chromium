// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_LOG_MESSAGE_LEVELS_H_
#define MEDIA_BASE_MEDIA_LOG_MESSAGE_LEVELS_H_

#include <string>

#include "media/base/media_export.h"

namespace media {

// TODO(tmathmeyer) Find a nice way to make this use the kCamelCase style, while
// still preserving the "MEDIA_LOG(ERROR, ...)" syntax. macros are bad :(
enum class MediaLogMessageLevel {
  kERROR,
  kWARNING,
  kINFO,
  kDEBUG,
};

MEDIA_EXPORT std::string MediaLogMessageLevelToString(
    MediaLogMessageLevel level);

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_LOG_MESSAGE_LEVELS_H_
