// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_SWITCHES_INTERNAL_H_
#define CONTENT_COMMON_CONTENT_SWITCHES_INTERNAL_H_

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

namespace base {
class CommandLine;
}

namespace content {

bool IsPinchToZoomEnabled();

blink::mojom::V8CacheOptions GetV8CacheOptions();

void WaitForDebugger(const std::string& label);

// Returns all comma-separated values from all instances of a switch, in the
// order they appear.  For example: given command line "--foo=aa,bb --foo=cc",
// the feature list for switch "foo" will be ["aa", "bb", "cc"].
CONTENT_EXPORT std::vector<std::string> FeaturesFromSwitch(
    const base::CommandLine& command_line,
    const char* switch_name);

} // namespace content

#endif  // CONTENT_COMMON_CONTENT_SWITCHES_INTERNAL_H_
