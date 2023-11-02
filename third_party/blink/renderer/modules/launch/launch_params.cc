// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/launch/launch_params.h"

#include "third_party/blink/renderer/modules/file_system_access/file_system_handle.h"

namespace blink {

LaunchParams::LaunchParams(KURL target_url)
    : target_url_(std::move(target_url)) {}

LaunchParams::LaunchParams(HeapVector<Member<FileSystemHandle>> files)
    : files_(std::move(files)) {}

LaunchParams::~LaunchParams() = default;

void LaunchParams::Trace(Visitor* visitor) const {
  visitor->Trace(files_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
