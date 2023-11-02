// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Intercept window.print calls.
 */

import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js'

// Overwrites window.print function to invoke chrome command.
window.print = function() {
  sendWebKitMessage('PrintMessageHandler', {});
};
