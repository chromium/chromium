// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_traceable_screenshot.h"

#include "base/base64.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace content {

base::subtle::Atomic32 DevToolsTraceableScreenshot::number_of_instances_ = 0;

// static
base::subtle::Atomic32 DevToolsTraceableScreenshot::GetNumberOfInstances() {
  return base::subtle::NoBarrier_Load(&number_of_instances_);
}

DevToolsTraceableScreenshot::DevToolsTraceableScreenshot(const SkBitmap& bitmap)
    : frame_(bitmap) {
  base::subtle::NoBarrier_AtomicIncrement(&number_of_instances_, 1);
}

DevToolsTraceableScreenshot::~DevToolsTraceableScreenshot() {
  base::subtle::NoBarrier_AtomicIncrement(&number_of_instances_, -1);
}

void DevToolsTraceableScreenshot::AppendAsTraceFormat(std::string* out) const {
  out->append("\"");
  if (!frame_.drawsNothing()) {
    std::optional<std::vector<uint8_t>> data =
        gfx::JPEGCodec::Encode(frame_, /*quality=*/80);
    if (data) {
      base::Base64EncodeAppend(data.value(), out);
    }
  }
  out->append("\"");
}

}  // namespace content
