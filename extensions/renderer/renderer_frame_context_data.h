// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_RENDERER_FRAME_CONTEXT_DATA_H_
#define EXTENSIONS_RENDERER_RENDERER_FRAME_CONTEXT_DATA_H_

#include <stdint.h>
#include <memory>

#include "base/memory/raw_ptr.h"
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
      : frame_(frame) {
    // Note: Extension tests mock objects like ScriptContext and don't fill in
    // their frame. This results in calls to this constructor with a nullptr
    // frame, so we can't CHECK(frame_) here.
  }

  ~RendererFrameContextData() override = default;

  std::unique_ptr<FrameContextData> CloneFrameContextData() const override;
  bool HasControlledFrameCapability() const override;

  std::unique_ptr<FrameContextData> GetLocalParentOrOpener() const override;

  GURL GetUrl() const override;
  url::Origin GetOrigin() const override;

  bool CanAccess(const url::Origin& target) const override;
  bool CanAccess(const FrameContextData& target) const override;

  uintptr_t GetId() const override;

 private:
  const raw_ptr<const blink::WebLocalFrame> frame_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_RENDERER_FRAME_CONTEXT_DATA_H_
