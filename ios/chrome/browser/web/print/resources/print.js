// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Intercept window.print calls.

/**
 * Namespace for this module.
 */
__gCrWeb['print'] = {};

new function() {
  // Overwrites window.print function to invoke chrome command.
  window.print = function() {
    __gCrWeb.common.sendWebKitMessage('PrintMessageHandler', {});
  };
}
