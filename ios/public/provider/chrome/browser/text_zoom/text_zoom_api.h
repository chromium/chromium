// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEXT_ZOOM_TEXT_ZOOM_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEXT_ZOOM_TEXT_ZOOM_API_H_

namespace web {
class WebState;
}  // namespace web

namespace ios {
namespace provider {

// Zooms the given web_state to the provided size as a percentage. I.e. a size
// of 100 corresponds to a zoom of 100%.
void SetTextZoomForWebState(web::WebState* web_state, int size);

// Returns whether text zoom is enabled currently.
bool IsTextZoomEnabled();

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEXT_ZOOM_TEXT_ZOOM_API_H_
