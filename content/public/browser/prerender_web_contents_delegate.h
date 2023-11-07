// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRERENDER_WEB_CONTENTS_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_PRERENDER_WEB_CONTENTS_DELEGATE_H_

#include "content/public/browser/web_contents_delegate.h"

namespace content {

// This is used as the delegate of WebContents created for a new tab where
// prerendering runs. The delegate will be swapped with a proper one on
// prerender page activation.
class CONTENT_EXPORT PrerenderWebContentsDelegate : public WebContentsDelegate {
 public:
  PrerenderWebContentsDelegate() = default;
  ~PrerenderWebContentsDelegate() override = default;

  // WebContentsDelegate overrides.
  void CloseContents(WebContents* source) override;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRERENDER_WEB_CONTENTS_DELEGATE_H_
