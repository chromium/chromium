// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/renderer_context_data.h"

#include "third_party/blink/public/web/blink.h"

namespace extensions {

std::unique_ptr<ContextData> RendererContextData::Clone() const {
  return std::make_unique<RendererContextData>();
}

bool RendererContextData::IsIsolatedApplication() const {
  return blink::IsIsolatedContext();
}

}  // namespace extensions
