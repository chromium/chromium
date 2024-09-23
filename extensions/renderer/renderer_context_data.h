// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_RENDERER_CONTEXT_DATA_H_
#define EXTENSIONS_RENDERER_RENDERER_CONTEXT_DATA_H_

#include "extensions/common/context_data.h"

namespace extensions {

class RendererContextData : public ContextData {
 public:
  // Returns true if the current context has an isolated context capability,
  // such as for an Isolated Web App. This static method is used in the
  // HasControlledFrameCapability() implementation for this class and
  // RendererFrameContextData.
  static bool IsIsolatedWebAppContextAndEnabled();

  RendererContextData() = default;
  ~RendererContextData() override = default;

  bool HasControlledFrameCapability() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_RENDERER_CONTEXT_DATA_H_
