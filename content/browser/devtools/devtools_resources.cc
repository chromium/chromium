// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/grit/devtools_resources.h"

#include "base/containers/span.h"
#include "content/browser/devtools/grit/devtools_resources_map.h"
#include "ui/base/webui/resource_path.h"

extern const int kCcompressedProtocolJSON = COMPRESSED_PROTOCOL_JSON;

namespace content {

base::span<const webui::ResourcePath> GetDevToolsResources() {
  return kDevtoolsResources;
}

}  // namespace content
