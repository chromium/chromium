// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_MANAGER_H_

#include "base/callback.h"
#include "content/common/content_export.h"

namespace content {

class WebContents;

// A BrowserPluginGuestManager offloads guest management and routing
// operations outside of the content layer.
class CONTENT_EXPORT BrowserPluginGuestManager {
 public:
  virtual ~BrowserPluginGuestManager() {}

  // Iterates over all WebContents belonging to a given |embedder_web_contents|,
  // calling |callback| for each. If one of the callbacks returns true, then
  // the iteration exits early.
  using GuestCallback = base::RepeatingCallback<bool(WebContents*)>;
  virtual bool ForEachGuest(WebContents* embedder_web_contents,
                            const GuestCallback& callback);

  // Returns the "full page" guest if there is one. That is, if there is a
  // single BrowserPlugin in the given embedder which takes up the full page,
  // then it is returned.
  virtual WebContents* GetFullPageGuest(WebContents* embedder_web_contents);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_MANAGER_H_
