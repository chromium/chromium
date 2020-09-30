// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROVIDERS_CHROMIUM_TEXT_ZOOM_PROVIDER_H_
#define IOS_CHROME_BROWSER_PROVIDERS_CHROMIUM_TEXT_ZOOM_PROVIDER_H_

#import "ios/public/provider/chrome/browser/text_zoom_provider.h"

class ChromiumTextZoomProvider : public TextZoomProvider {
 public:
  ChromiumTextZoomProvider();
  ~ChromiumTextZoomProvider() override;

 private:
  // Text Zoom Provider
  void SetPageFontSize(web::WebState* web_state, int size) override;
  bool IsTextZoomEnabled() override;
};

#endif  // IOS_CHROME_BROWSER_PROVIDERS_CHROMIUM_TEXT_ZOOM_PROVIDER_H_
