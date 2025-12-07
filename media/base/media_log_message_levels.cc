// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_log_message_levels.h"

#include <string>

#include "base/notreached.h"

namespace media {

std::string MediaLogMessageLevelToString(MediaLogMessageLevel level) {
  switch (level) {
    case MediaLogMessageLevel::kERROR:
      return "error";
    case MediaLogMessageLevel::kWARNING:
      return "warning";
    case MediaLogMessageLevel::kINFO:
      return "info";
    case MediaLogMessageLevel::kDEBUG:
      return "debug";
  }
  NOTREACHED();
}

}  // namespace media