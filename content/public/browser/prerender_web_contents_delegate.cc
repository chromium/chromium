// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/prerender_web_contents_delegate.h"

#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

void PrerenderWebContentsDelegate::CloseContents(WebContents* source) {
  // Cancelling prerendering should eventually destroy `this` and `source`.
  // However, this behavior is not implemented yet.
  // TODO(https://crbug.com/1499759): Implement this behavior.
  static_cast<WebContentsImpl*>(source)
      ->GetPrerenderHostRegistry()
      ->CancelAllHosts(PrerenderFinalStatus::kTabClosedWithoutUserGesture);
}

}  // namespace content
