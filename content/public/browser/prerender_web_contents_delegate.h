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
//
// This class is currently empty but necessary to bypass the "protected"
// destructor of WebContentsDelegate. Without this indirect class, an owner of
// WebContentsDelegate implementation in content/ cannot call the destructor.
// See review comments on https://crrev.com/c/4680866/ for details.
class PrerenderWebContentsDelegate : public WebContentsDelegate {};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRERENDER_WEB_CONTENTS_DELEGATE_H_
