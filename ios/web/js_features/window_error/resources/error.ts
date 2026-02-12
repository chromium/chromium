// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Error listener to report error details to the native app.
 */

import {CrWebError} from '//ios/web/public/js_messaging/resources/gcrweb_error.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * JavaScript errors are logged on the main application side. The handler is
 * added ASAP to catch any errors in startup.
 */
function errorEventHandler(event: ErrorEvent): void {
  // CrWebError errors will be reported directly because they are triggered from
  // native API calls and will provide better error details than the error
  // handler here. Early return to prevent double reporting those errors.
  if (event instanceof CrWebError) {
    return;
  }

  sendWebKitMessage('WindowErrorResultHandler', {
    'line_number': event.lineno,
    // The JS error handler is limited in the error details it can access and is
    // likely to only have the message "Script error". Thus, this is a last
    // resort error reporting mechanism for errors which were not caught. Yet,
    // reporting these errors are important, especially for tests where
    // `kAssertOnJavaScriptErrors` may be enabled.
    'message': event.message.toString(),
  });
}


window.addEventListener('error', errorEventHandler);
