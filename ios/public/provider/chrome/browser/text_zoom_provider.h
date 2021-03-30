// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEXT_ZOOM_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEXT_ZOOM_PROVIDER_H_

namespace web {
class WebState;
}

class TextZoomProvider {
 public:
  TextZoomProvider();
  virtual ~TextZoomProvider();

  // Zooms the given web_state to the provided size as a percentage. I.e. a size
  // of 100 corresponds to a zoom of 100%.
  virtual void SetPageFontSize(web::WebState* web_state, int size);

  // Returns whether text zoom is enabled currently.
  virtual bool IsTextZoomEnabled();
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEXT_ZOOM_PROVIDER_H_
