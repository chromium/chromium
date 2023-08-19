// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Test that the webview JS implementation does not inadvertently call user
// code.
//
// Our implementation should use extensions::SafeBuiltins or otherwise
// keep references to the real methods to avoid calling overwritten methods.
// Our internal objects can be modified to not inherit from Object in order
// to avoid calling getters and setters if a property name is defined on
// Object.prototype.
//
// Note that the properties and methods we taint are not exhaustive.

window.onload = () => {
  chrome.test.sendMessage('LAUNCHED');
};

// These are needed for the test itself, so keep a reference to the real method.
EventTarget.prototype.savedAddEventListener =
    EventTarget.prototype.addEventListener;
Node.prototype.savedAppendChild = Node.prototype.appendChild;

function makeUnreached() {
  return function unreachableFunction() {
    chrome.test.fail('Reached unreachable code');
  };
}

(function taintProperties() {
  var properties = [
    'AppView',
    'WebView',
    '__proto__',
    'actionQueue',
    'allowscaling',
    'allowtransparency',
    'app',
    'appview',
    'attributes',
    'autosize',
    'border',
    'cancelable',
    'constructor',
    'contentWindow',
    'data',
    'defaultView',
    'dirty',
    'element',
    'elementHeight',
    'errorNode',
    'events',
    'guest',
    'guestView',
    'height',
    'initialZoomFactor',
    'innerText',
    'instanceId',
    'internal',
    'internalInstanceId',
    'left',
    'listener',
    'loadstop',
    'maxheight',
    'newHeight',
    'on',
    'onloadstop',
    'onresize',
    'ownerDocument',
    'parentNode',
    'partition',
    'pendingAction',
    'position',
    'processId',
    'prototype',
    'shadowRoot',
    'src',
    'state',
    'style',
    'top',
    'userAgentOverride',
    'validPartitionId',
    'view',
    'viewInstanceId',
    'viewType',
    'webview',
  ];
  // For objects that don't inherit directly from Object, we'll need to taint
  // existing properties on prototypes earlier in the prototype chain.
  var otherConstructors = [
    Document,
    Element,
    HTMLElement,
    HTMLIFrameElement,
    Node,
  ];
  for (var property of properties) {
    Object.defineProperty(Object.prototype, property, {
      get: makeUnreached(),
      set: makeUnreached(),
    });
    for (var constructor of otherConstructors) {
      if (constructor.prototype.hasOwnProperty(property)) {
        Object.defineProperty(constructor.prototype, property, {
          get: makeUnreached(),
          set: makeUnreached(),
        });
      }
    }
  }
})();

// Overwrite methods.
Object.assign = makeUnreached();
Object.create = makeUnreached();
Object.defineProperty = makeUnreached();
Object.freeze = makeUnreached();
Object.getOwnPropertyDescriptor = makeUnreached();
Object.getPrototypeOf = makeUnreached();
Object.keys = makeUnreached();
Object.setPrototypeOf = makeUnreached();
Object.prototype.hasOwnProperty = makeUnreached();
Function.prototype.apply = makeUnreached();
Function.prototype.bind = makeUnreached();
Function.prototype.call = makeUnreached();
Array.from = makeUnreached();
Array.isArray = makeUnreached();
Array.prototype.concat = makeUnreached();
Array.prototype.filter = makeUnreached();
Array.prototype.forEach = makeUnreached();
Array.prototype.indexOf = makeUnreached();
Array.prototype.join = makeUnreached();
Array.prototype.map = makeUnreached();
Array.prototype.pop = makeUnreached();
Array.prototype.push = makeUnreached();
Array.prototype.reverse = makeUnreached();
Array.prototype.shift = makeUnreached();
Array.prototype.slice = makeUnreached();
Array.prototype.splice = makeUnreached();
Array.prototype.unshift = makeUnreached();
String.prototype.indexOf = makeUnreached();
String.prototype.replace = makeUnreached();
String.prototype.slice = makeUnreached();
String.prototype.split = makeUnreached();
String.prototype.substr = makeUnreached();
String.prototype.toLowerCase = makeUnreached();
String.prototype.toUpperCase = makeUnreached();

CustomElementRegistry.prototype.define = makeUnreached();
Document.prototype.createElement = makeUnreached();
Document.prototype.createEvent = makeUnreached();
Element.prototype.attachShadow = makeUnreached();
Element.prototype.getAttribute = makeUnreached();
Element.prototype.getBoundingClientRect = makeUnreached();
Element.prototype.hasAttribute = makeUnreached();
Element.prototype.removeAttribute = makeUnreached();
Element.prototype.setAttribute = makeUnreached();
EventTarget.prototype.addEventListener = makeUnreached();
EventTarget.prototype.dispatchEvent = makeUnreached();
EventTarget.prototype.removeEventListener = makeUnreached();
HTMLElement.prototype.focus = makeUnreached();
MutationObserver.prototype.observe = makeUnreached();
MutationObserver.prototype.takeRecords = makeUnreached();
Node.prototype.appendChild = makeUnreached();
Node.prototype.removeChild = makeUnreached();
Node.prototype.replaceChild = makeUnreached();

getComputedStyle = makeUnreached();
parseInt = makeUnreached();
parseFloat = makeUnreached();

// Also overwrite constructors.
MutationObserver = makeUnreached();
Object = makeUnreached();
Function = makeUnreached();
Array = makeUnreached();
String = makeUnreached();

var tests = {
  testCreate: () => {
    var webview = new WebView();
    webview.src = 'data:text/html,<body>Guest</body>';
    webview.savedAddEventListener('loadstop', chrome.test.callbackPass());
    document.body.savedAppendChild(webview);
  },

  testSetOnEventProperty: () => {
    var webview = new WebView();
    // Set and overwrite an on<event> property on the view.
    webview.onloadstop = () => {};
    webview.onloadstop = () => {};
    chrome.test.succeed();
  },

  testGetSetAttributes: () => {
    var webview = new WebView();

    // Get and set various attribute types.
    var url = 'data:text/html,<body>Guest</body>';
    webview.src = url;
    chrome.test.assertEq(url, webview.src);

    webview.autosize = true;
    chrome.test.assertTrue(webview.autosize);
    webview.autosize = false;
    chrome.test.assertFalse(webview.autosize);

    webview.maxheight = 123;
    chrome.test.assertEq(123, webview.maxheight);
    webview.maxheight = undefined;
    chrome.test.assertEq(0, webview.maxheight);

    var name = 'my-webview';
    webview.name = name;
    chrome.test.assertEq(name, webview.name);
    webview.name = undefined;
    chrome.test.assertEq('', webview.name);

    chrome.test.succeed();
  },

  testBackForward: () => {
    var webview = new WebView();
    // The back and forward methods are implemented in terms of go. Make sure
    // they don't call an overwritten version.
    webview.go = makeUnreached();
    webview.back();
    webview.forward();
    chrome.test.succeed();
  },

  testFocus: () => {
    var webview = new WebView();
    webview.src = 'data:text/html,<body>Guest</body>';
    webview.savedAddEventListener('loadstop', chrome.test.callbackPass(() => {
      webview.focus();
    }));
    document.body.savedAppendChild(webview);
  },
};

window.runTest = (testName) => {
  if (!tests[testName]) {
    chrome.test.notifyFail('Test does not exist: ' + testName);
    return;
  }

  chrome.test.runTests([tests[testName]]);
};
