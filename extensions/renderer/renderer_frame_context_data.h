// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_RENDERER_FRAME_CONTEXT_DATA_H_
#define EXTENSIONS_RENDERER_RENDERER_FRAME_CONTEXT_DATA_H_

#include <stdint.h>
#include <memory>

#include "extensions/common/frame_context_data.h"
#include "third_party/blink/public/web/web_local_frame.h"

class GURL;

namespace url {
class Origin;
}  // namespace url

namespace extensions {

class RendererFrameContextData : public FrameContextData {
 public:
  explicit RendererFrameContextData(const blink::WebLocalFrame* frame)
      : frame_(frame) {}

  ~RendererFrameContextData() override = default;

  std::unique_ptr<ContextData> Clone() const override;
  std::unique_ptr<FrameContextData> CloneFrameContextData() const override;
  bool IsIsolatedApplication() const override;

  std::unique_ptr<FrameContextData> GetLocalParentOrOpener() const override;

  GURL GetUrl() const override;
  url::Origin GetOrigin() const override;

  bool CanAccess(const url::Origin& target) const override;
  bool CanAccess(const FrameContextData& target) const override;

  uintptr_t GetId() const override;

 private:
  const blink::WebLocalFrame* const frame_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_RENDERER_FRAME_CONTEXT_DATA_H_
