// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_RENDERER_CONTEXT_DATA_H_
#define EXTENSIONS_RENDERER_RENDERER_CONTEXT_DATA_H_

#include "extensions/common/context_data.h"

namespace extensions {

class RendererContextData : public ContextData {
 public:
  // Returns true if the kIsolatedWebApps feature flag is enabled and the
  // current context is for an Isolated Web App. This static method is used in
  // the IsIsolatedApplication() implementation for this class and
  // RendererFrameContextData.
  static bool IsIsolatedWebAppContextAndEnabled();

  RendererContextData() = default;
  ~RendererContextData() override = default;

  std::unique_ptr<ContextData> Clone() const override;
  bool IsIsolatedApplication() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_RENDERER_CONTEXT_DATA_H_
