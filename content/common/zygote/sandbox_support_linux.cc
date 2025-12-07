// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/zygote/sandbox_support_linux.h"

#include "base/posix/global_descriptors.h"
#include "content/public/common/content_descriptors.h"

namespace content {

int GetSandboxFD() {
  return kSandboxIPCChannel + base::GlobalDescriptors::kBaseDescriptor;
}

}  // namespace content
