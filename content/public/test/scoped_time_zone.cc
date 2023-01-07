// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/scoped_time_zone.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"

namespace content {

ScopedTimeZone::ScopedTimeZone(const char* new_zoneid)
    : icu_time_zone_(new_zoneid) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kTimeZoneForTesting, new_zoneid);
}

ScopedTimeZone::~ScopedTimeZone() = default;

}  // namespace content
