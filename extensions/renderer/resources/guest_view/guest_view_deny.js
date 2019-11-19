// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the registration of guestview elements when
// permissions are not available. These elements exist only to provide a useful
// error message when developers attempt to use them.

var $CustomElementRegistry =
    require('safeMethods').SafeMethods.$CustomElementRegistry;
var $EventTarget = require('safeMethods').SafeMethods.$EventTarget;
var GuestViewInternalNatives = requireNative('guest_view_internal');

// Once the document has loaded, expose the error-providing element's
// constructor to user code via |window|.
// GuestView elements used to be defined only once the document had loaded (see
// https://crbug.com/810012). This has been fixed, but as seen in
// https://crbug.com/1014385, user code that does not have permission for a
// GuestView could be using the same name for another purpose. In order to avoid
// potential name collisions with user code, we preserve the previous
// asynchronous behaviour for exposing the constructor of the error-providing
// element via |window|.
function asyncProvideElementConstructor(viewType, elementConstructor) {
  let useCapture = true;
  window.addEventListener('readystatechange', function listener(event) {
    if (document.readyState == 'loading')
      return;

    // If user code did use the name, we won't overwrite with the
    // error-providing element.
    if (!$Object.hasOwnProperty(window, viewType)) {
      $Object.defineProperty(window, viewType, {
        value: elementConstructor,
      });
    }

    $EventTarget.removeEventListener(window, event.type, listener, useCapture);
  }, useCapture);
}

// Registers an error-providing GuestView custom element.
function registerDeniedElement(viewType, permissionName) {
  GuestViewInternalNatives.AllowGuestViewElementDefinition(() => {
    var DeniedElement = class extends HTMLElement {
      constructor() {
        super();
        window.console.error(`You do not have permission to use the ${
            viewType} element. Be sure to declare the "${
            permissionName}" permission in your manifest file.`);
      }
    }
    $CustomElementRegistry.define(
        window.customElements, $String.toLowerCase(viewType), DeniedElement);
    asyncProvideElementConstructor(viewType, DeniedElement);
  });
}

// Exports.
exports.$set('registerDeniedElement', registerDeniedElement);
