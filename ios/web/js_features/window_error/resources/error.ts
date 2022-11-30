// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Error listener to report error details to the native app.
 */

import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js'

/**
 * JavaScript errors are logged on the main application side. The handler is
 * added ASAP to catch any errors in startup.
 */
function errorEventHandler(event: ErrorEvent): void {
  sendWebKitMessage('WindowErrorResultHandler',
      {'filename' : event.filename,
       'line_number' : event.lineno,
       'message': event.message.toString()
      });
}


window.addEventListener('error', errorEventHandler);
