// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Gets and returns favicons in main frame. This script should be
 * injected only into main frame when it's loaded.
 */

goog.provide('__crWeb.favicon');

// Requires __crWeb.common and __crWeb.message provided by
// __crWeb.allFramesWebBundle.

/** Beginning of anonymous object */
(function() {

__gCrWeb.message.invokeOnHost({
  'command': 'favicon.favicons',
  'favicons': __gCrWeb.common.getFavicons()
});
}());  // End of anonymous object
