// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SECURITY_WEB_INTERSTITIAL_DELEGATE_H_
#define IOS_WEB_PUBLIC_SECURITY_WEB_INTERSTITIAL_DELEGATE_H_

#include <string>

@class UIColor;
@class UIView;

namespace web {

class NavigationItem;

// Superclass for delegates that provide data to a WebInterstitial.  After the
// WebInterstitial is shown, it takes ownership of its delegate.
class WebInterstitialDelegate {
 public:
  virtual ~WebInterstitialDelegate() {}

  // Called when the interstitial is proceeded or cancelled. Note that this may
  // be called directly even if the embedder didn't call Proceed or DontProceed
  // on WebInterstitial, since navigations etc may cancel them.
  virtual void OnProceed() {}
  virtual void OnDontProceed() {}

  // Called with the NavigationItem that is going to be added to the navigation
  // manager.
  // Gives an opportunity to delegates to set states on the |item|.
  // Note that this is only called if the WebInterstitial was constructed with
  // |new_navigation| set to true.
  virtual void OverrideItem(NavigationItem* item) {}

  // Returns the HTML that should be displayed in the page.
  virtual std::string GetHtmlContents() const = 0;

  // Invoked when a WebInterstitial receives a command via JavaScript.
  virtual void CommandReceived(const std::string& command) {}
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_SECURITY_WEB_INTERSTITIAL_DELEGATE_H_
