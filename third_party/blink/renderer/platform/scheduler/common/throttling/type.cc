// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/throttling/type.h"

namespace blink::scheduler {

const char* ThrottlingTypeToString(ThrottlingType type) {
  switch (type) {
    case ThrottlingType::kNone:
      return "none";
    case ThrottlingType::kForegroundUnimportant:
      return "foreground-unimportant";
    case ThrottlingType::kBackground:
      return "background";
    case ThrottlingType::kBackgroundIntensive:
      return "background-intensive";
  }
}

}  // namespace blink::scheduler
