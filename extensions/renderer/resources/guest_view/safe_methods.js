// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module keeps references to original methods before user code is able
// to overwrite them. We assume that this module is executed before any user
// code. The idea is similar to the extension system's SafeBuiltins, and since
// it's similar, we also use a $ prefix as a naming convention.
// For example,
// myElement.setAttribute(name, value);
// becomes
// $Element.setAttribute(myElement, name, value);
// We also provide access to getters and setters:
// myNode.parentNode;
// becomes
// $Node.parentNode.get(myNode);

function makeCallable(prototypeMethod) {
  return (thisArg, ...args) => {
    return $Function.apply(prototypeMethod, thisArg, args);
  };
}

function saveMethods(original, safe, methods) {
  for (var method of methods) {
    safe[method] = makeCallable(original.prototype[method]);
  }
}

function saveAccessors(original, safe, properties) {
  for (var property of properties) {
    var desc = $Object.getOwnPropertyDescriptor(original.prototype, property);

    safe[property] = {
      get: desc.get && makeCallable(desc.get),
      set: desc.set && makeCallable(desc.set),
    };
  }
}

var SafeMethods = {
  $CustomElementRegistry: {},
  $Document: {},
  $Element: {},
  $EventTarget: {},
  $HTMLElement: {},
  $HTMLIFrameElement: {},
  $MutationObserver: MutationObserver,
  $Node: {},
  $getComputedStyle: window.getComputedStyle,
  $parseInt: window.parseInt,
};

saveMethods(CustomElementRegistry, SafeMethods.$CustomElementRegistry, [
  'define',
]);

saveMethods(Document, SafeMethods.$Document, [
  'createElement',
  'webkitCancelFullScreen',
]);

saveAccessors(Document, SafeMethods.$Document, [
  'defaultView',
]);

saveMethods(Element, SafeMethods.$Element, [
  'attachShadow',
  'getAttribute',
  'getBoundingClientRect',
  'hasAttribute',
  'removeAttribute',
  'setAttribute',
  'webkitRequestFullScreen',
]);

saveMethods(EventTarget, SafeMethods.$EventTarget, [
  'addEventListener',
  'dispatchEvent',
  'removeEventListener',
]);

saveMethods(HTMLElement, SafeMethods.$HTMLElement, [
  'focus',
]);

saveAccessors(HTMLElement, SafeMethods.$HTMLElement, [
  'style',
  'innerText',
]);

saveAccessors(HTMLIFrameElement, SafeMethods.$HTMLIFrameElement, [
  'contentWindow',
]);

saveMethods(MutationObserver, SafeMethods.$MutationObserver, [
  'observe',
  'takeRecords',
]);

saveMethods(Node, SafeMethods.$Node, [
  'appendChild',
  'replaceChild',
]);

saveAccessors(Node, SafeMethods.$Node, [
  'parentNode',
  'ownerDocument',
]);

exports.$set('SafeMethods', SafeMethods);
