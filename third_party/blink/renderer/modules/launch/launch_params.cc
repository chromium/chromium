// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/launch/launch_params.h"

#include "base/time/time.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_handle.h"

namespace blink {

LaunchParams::LaunchParams(KURL target_url,
                           base::TimeTicks time_navigation_started_in_browser,
                           bool navigation_started)
    : target_url_(std::move(target_url)),
      time_navigation_started_in_browser_(time_navigation_started_in_browser),
      navigation_started_(navigation_started) {}

LaunchParams::LaunchParams(HeapVector<Member<FileSystemHandle>> files)
    : files_(std::move(files)) {}

LaunchParams::~LaunchParams() = default;

void LaunchParams::Trace(Visitor* visitor) const {
  visitor->Trace(files_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
