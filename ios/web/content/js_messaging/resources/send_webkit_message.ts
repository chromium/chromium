// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

type WindowWithExtras = Window&(typeof globalThis)&{
  __proto__: any,
  webkitMessageHandler: any,
};

/**
 * Injects a proxy object that captures calls to window.webkit.messageHandlers
 * and forwards them to window.webkitMessageHandler.
 */
(window as WindowWithExtras).__proto__.webkit = {};
(window as WindowWithExtras).__proto__.webkit.messageHandlers =
    new Proxy((window as WindowWithExtras).webkitMessageHandler, {
      get(target, property) {
        return {
          'postMessage': function(message: object|string) {
            const payload = {'handler_name': property, 'message': message};
            target.postMessage(JSON.stringify(payload));
          },
        };
      },
    });
