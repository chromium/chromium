// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_HIDDEN_TARGET_MANAGER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_HIDDEN_TARGET_MANAGER_H_

#include "base/containers/unique_ptr_adapters.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/public/browser/web_contents_delegate.h"

namespace content::protocol {

// Manages hidden targets. Owns hidden web contents created for hidden targets
// and disposes them when session is closed or when the delegate's close is
// called. It is also the `WebContentsDelegate` for the hidden targets to
// properly dispose them.
class HiddenTargetManager : public content::WebContentsDelegate {
 public:
  HiddenTargetManager();
  ~HiddenTargetManager() override;
  HiddenTargetManager(const HiddenTargetManager&) = delete;
  HiddenTargetManager& operator=(const HiddenTargetManager&) = delete;

  // Disposes all hidden web contents.
  void Clear();
  std::string CreateHiddenTarget(const GURL& url,
                                 BrowserContext* browser_context);

  // Implements `WebContentsDelegate::CloseContents` for hidden web contents.
  void CloseContents(content::WebContents* source) override;

 private:
  // The map of hidden web contents created for hidden targets.
  // HiddenTargetManager owns them and disposes when session is closed or when
  // the delegate's close is called.

  base::flat_set<std::unique_ptr<content::WebContents>,
                 base::UniquePtrComparator>
      hidden_web_contents_;
};

}  // namespace content::protocol

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_HIDDEN_TARGET_MANAGER_H_
