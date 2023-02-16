// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_BROWSER_CONTEXT_DATA_H_
#define EXTENSIONS_BROWSER_BROWSER_CONTEXT_DATA_H_

#include <memory>

#include "extensions/common/context_data.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
}

namespace extensions {

class BrowserContextData : public ContextData {
 public:
  explicit BrowserContextData(content::RenderFrameHost* frame)
      : frame_(frame) {}

  ~BrowserContextData() override = default;

  std::unique_ptr<ContextData> Clone() const override;
  std::unique_ptr<ContextData> GetLocalParentOrOpener() const override;

  GURL GetUrl() const override;
  url::Origin GetOrigin() const override;

  bool CanAccess(const url::Origin& target) const override;
  bool CanAccess(const ContextData& target) const override;

  uintptr_t GetId() const override;

 private:
  const raw_ptr<content::RenderFrameHost> frame_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_BROWSER_CONTEXT_DATA_H_
