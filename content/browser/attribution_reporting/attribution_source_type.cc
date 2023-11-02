// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_source_type.h"

namespace content {

const char* AttributionSourceTypeToString(AttributionSourceType source_type) {
  switch (source_type) {
    case AttributionSourceType::kNavigation:
      return "navigation";
    case AttributionSourceType::kEvent:
      return "event";
  }
}

}  // namespace content
