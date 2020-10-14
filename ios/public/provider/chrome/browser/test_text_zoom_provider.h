// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEST_TEXT_ZOOM_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEST_TEXT_ZOOM_PROVIDER_H_

#import "ios/public/provider/chrome/browser/text_zoom_provider.h"

class TestTextZoomProvider : public TextZoomProvider {
 public:
  TestTextZoomProvider();
  ~TestTextZoomProvider() override;

 private:
  bool IsTextZoomEnabled() override;
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEST_TEXT_ZOOM_PROVIDER_H_
