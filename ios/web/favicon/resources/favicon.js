// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Gets and returns favicons in main frame. This script should be
 * injected only into main frame when it's loaded.
 */

// Requires functions from common.js

// Store namespace object in a global __gCrWeb object referenced by a string, so
// it does not get renamed by closure compiler during the minification.
__gCrWeb.favicon = {};
__gCrWeb['favicon'] = __gCrWeb.favicon;

/**
 * Retrieves favicon information.
 *
 * @return {Object} Object containing favicon data.
 */
function getFavicons() {
  var favicons = [];
  delete favicons.toJSON;  // Never inherit Array.prototype.toJSON.
  var links = document.getElementsByTagName('link');
  var linkCount = links.length;
  for (var i = 0; i < linkCount; ++i) {
    if (links[i].rel) {
      var rel = links[i].rel.toLowerCase();
      if (rel == 'shortcut icon' || rel == 'icon' ||
          rel == 'apple-touch-icon' || rel == 'apple-touch-icon-precomposed') {
        var favicon = {rel: links[i].rel.toLowerCase(), href: links[i].href};
        if (links[i].sizes && links[i].sizes.value) {
          favicon.sizes = links[i].sizes.value;
        }
        favicons.push(favicon);
      }
    }
  }
  return favicons;
}

__gCrWeb.favicon.sendFaviconUrls = function() {
  __gCrWeb.common.sendWebKitMessage('FaviconUrlsHandler', getFavicons() );
};

__gCrWeb.favicon.sendFaviconUrls();
