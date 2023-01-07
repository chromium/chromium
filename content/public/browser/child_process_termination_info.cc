// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/child_process_termination_info.h"

namespace content {

ChildProcessTerminationInfo::ChildProcessTerminationInfo() = default;
ChildProcessTerminationInfo::ChildProcessTerminationInfo(
    const ChildProcessTerminationInfo& other) = default;
ChildProcessTerminationInfo::~ChildProcessTerminationInfo() = default;

}  // namespace content
