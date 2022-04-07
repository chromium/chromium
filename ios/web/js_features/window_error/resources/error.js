// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Error listener to report error details to the native app.
 */

// Requires functions from common.js

/**
 * JavaScript errors are logged on the main application side. The handler is
 * added ASAP to catch any errors in startup.
 */
window.addEventListener('error', function(event) {
  __gCrWeb.common.sendWebKitMessage('WindowErrorResultHandler',
      {'filename' : event.filename,
       'line_number' : event.lineno,
       'message': event.message.toString()
      });
});
