// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_BROWSER_FRAME_CONTEXT_DATA_H_
#define EXTENSIONS_BROWSER_BROWSER_FRAME_CONTEXT_DATA_H_

#include <memory>

#include "extensions/common/frame_context_data.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
}

namespace extensions {

class BrowserFrameContextData : public FrameContextData {
 public:
  explicit BrowserFrameContextData(content::RenderFrameHost* frame)
      : frame_(frame) {}

  ~BrowserFrameContextData() override = default;

  std::unique_ptr<FrameContextData> CloneFrameContextData() const override;
  bool HasControlledFrameCapability() const override;

  std::unique_ptr<FrameContextData> GetLocalParentOrOpener() const override;

  GURL GetUrl() const override;
  url::Origin GetOrigin() const override;

  bool CanAccess(const url::Origin& target) const override;
  bool CanAccess(const FrameContextData& target) const override;

  uintptr_t GetId() const override;

 private:
  const raw_ptr<content::RenderFrameHost> frame_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_BROWSER_FRAME_CONTEXT_DATA_H_
