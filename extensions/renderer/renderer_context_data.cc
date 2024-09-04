// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/renderer_context_data.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/web/blink.h"

namespace extensions {

// static
bool RendererContextData::IsIsolatedWebAppContextAndEnabled() {
  return blink::IsIsolatedContext();
}

bool RendererContextData::HasControlledFrameCapability() const {
  return RendererContextData::IsIsolatedWebAppContextAndEnabled();
}

}  // namespace extensions
