// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

interface Window {
  __proto__: any;
  webkitMessageHandler: any;
}

/**
 * Injects a proxy object that captures calls to window.webkit.messageHandlers
 * and forwards them to window.webkitMessageHandler.
 */
window.__proto__.webkit = {};
window.__proto__.webkit.messageHandlers =
    new Proxy(window.webkitMessageHandler, {
      get(target, property) {
        return {
          'postMessage': function(message: object|string) {
            var payload = {'handler_name': property, 'message': message};
            target.postMessage(JSON.stringify(payload));
          }
        }
      }
    });
