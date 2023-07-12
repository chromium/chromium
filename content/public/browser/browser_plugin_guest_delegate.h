// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents.h"

namespace content {
class RenderFrameHost;

// Objects implement this interface to get notified about changes in the guest
// WebContents and to provide necessary functionality.
class CONTENT_EXPORT BrowserPluginGuestDelegate {
 public:
  virtual ~BrowserPluginGuestDelegate() = default;

  virtual std::unique_ptr<WebContents> CreateNewGuestWindow(
      const WebContents::CreateParams& create_params);

  // Returns the WebContents that currently owns this guest.
  virtual WebContents* GetOwnerWebContents();

  // Returns the RenderFrameHost that owns this guest, but has not yet attached
  // it.
  virtual RenderFrameHost* GetProspectiveOuterDocument();

  virtual base::WeakPtr<BrowserPluginGuestDelegate> GetGuestDelegateWeakPtr();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_
