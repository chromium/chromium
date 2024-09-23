// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_WEB_PROVIDER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_WEB_PROVIDER_H_

namespace web {
class WebState;
}  // namespace web

/// Provider for the lens result page web state. This is used to get the web
/// state from the lens overlay coordinator.
@protocol LensWebProvider

/// Returns the current web state.
- (web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_WEB_PROVIDER_H_
