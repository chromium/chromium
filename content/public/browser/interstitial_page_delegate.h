// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_INTERSTITIAL_PAGE_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_INTERSTITIAL_PAGE_DELEGATE_H_

#include <string>

#include "content/common/content_export.h"

namespace blink {
namespace mojom {
class RendererPreferences;
}
}  // namespace blink

namespace content {

class NavigationEntry;

// Controls and provides the html for an interstitial page. The delegate is
// owned by the InterstitialPage.
class CONTENT_EXPORT InterstitialPageDelegate {
 public:
  // An identifier used to identify an InterstitialPage.
  typedef const void* TypeID;

  virtual ~InterstitialPageDelegate() {}

  // Return the HTML that should be displayed in the page.
  virtual std::string GetHTMLContents() = 0;

  // Called when the interstitial is proceeded or cancelled. Note that this may
  // be called by content directly even if the embedder didn't call Proceed or
  // DontProceed on InterstitialPage, since navigations etc may cancel them.
  virtual void OnProceed() {}
  virtual void OnDontProceed() {}

  // Invoked when the page sent a command through DOMAutomation.
  virtual void CommandReceived(const std::string& command) {}

  // Invoked with the NavigationEntry that is going to be added to the
  // navigation controller.
  // Gives an opportunity to delegates to set states on the |entry|.
  // Note that this is only called if the InterstitialPage was constructed with
  // |new_navigation| set to true.
  virtual void OverrideEntry(content::NavigationEntry* entry) {}

  // Allows the delegate to override the renderer preferences structure that's
  // sent to the new RenderViewHost.
  virtual void OverrideRendererPrefs(blink::mojom::RendererPreferences* prefs) {
  }

  // Return the interstitial type for testing.
  virtual TypeID GetTypeForTesting();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_INTERSTITIAL_PAGE_DELEGATE_H_
