// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_MANAGER_H_

#include "base/functional/function_ref.h"
#include "content/common/content_export.h"

namespace content {

class WebContents;

// A BrowserPluginGuestManager offloads guest management and routing
// operations outside of the content layer.
class CONTENT_EXPORT BrowserPluginGuestManager {
 public:
  virtual ~BrowserPluginGuestManager() = default;

  // Iterates over guest WebContents that belong to a given
  // |owner_web_contents|, but have not yet been attached.
  virtual void ForEachUnattachedGuest(
      WebContents* owner_web_contents,
      base::FunctionRef<void(WebContents*)> fn) {}

  // Prefer using |RenderFrameHost::ForEachRenderFrameHost|.
  // Iterates over all WebContents belonging to a given |owner_web_contents|,
  // calling |fn| for each. If an invocation of `fn` returns true, the iteration
  // exits early.
  virtual bool ForEachGuest(WebContents* owner_web_contents,
                            base::FunctionRef<bool(WebContents*)> fn);

  // Returns the "full page" guest if there is one. That is, if there is a
  // single BrowserPlugin in the given embedder which takes up the full page,
  // then it is returned.
  virtual WebContents* GetFullPageGuest(WebContents* embedder_web_contents);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_MANAGER_H_
