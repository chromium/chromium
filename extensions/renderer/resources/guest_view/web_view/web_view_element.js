// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The <webview> custom element. This serves as a base implementation used
// to define <webview> at the extensions layer and may be extended by an
// embedder that wants to define its own <webview>.

var forwardApiMethods = require('guestViewContainerElement').forwardApiMethods;
var GuestViewContainerElement =
    require('guestViewContainerElement').GuestViewContainerElement;
var WebViewImpl = require('webView').WebViewImpl;
var WEB_VIEW_API_METHODS = require('webViewApiMethods').WEB_VIEW_API_METHODS;
var WebViewInternal = getInternalApi('webViewInternal');

class WebViewElement extends GuestViewContainerElement {}

WebViewElement.prototype.addContentScripts = function(rules) {
  var internal = privates(this).internal;
  return WebViewInternal.addContentScripts(internal.viewInstanceId, rules);
};

WebViewElement.prototype.removeContentScripts = function(names) {
  var internal = privates(this).internal;
  return WebViewInternal.removeContentScripts(internal.viewInstanceId, names);
};

WebViewElement.prototype.insertCSS = function(var_args) {
  var internal = privates(this).internal;
  return internal.executeCode(
      WebViewInternal.insertCSS, $Array.slice(arguments));
};

WebViewElement.prototype.executeScript = function(var_args) {
  var internal = privates(this).internal;
  return internal.executeCode(
      WebViewInternal.executeScript, $Array.slice(arguments));
};

WebViewElement.prototype.print = function() {
  var internal = privates(this).internal;
  return internal.executeCode(
      WebViewInternal.executeScript, [{code: 'window.print();'}]);
};

WebViewElement.prototype.back = function(callback) {
  return $Function.call(originalGo, this, -1, callback);
};

WebViewElement.prototype.canGoBack = function() {
  var internal = privates(this).internal;
  return internal.entryCount > 1 && internal.currentEntryIndex > 0;
};

WebViewElement.prototype.canGoForward = function() {
  var internal = privates(this).internal;
  return internal.currentEntryIndex >= 0 &&
      internal.currentEntryIndex < (internal.entryCount - 1);
};

WebViewElement.prototype.forward = function(callback) {
  return $Function.call(originalGo, this, 1, callback);
};

WebViewElement.prototype.getProcessId = function() {
  var internal = privates(this).internal;
  return internal.processId;
};

WebViewElement.prototype.getUserAgent = function() {
  var internal = privates(this).internal;
  return internal.userAgentOverride || navigator.userAgent;
};

WebViewElement.prototype.isUserAgentOverridden = function() {
  var internal = privates(this).internal;
  return !!internal.userAgentOverride &&
      internal.userAgentOverride != navigator.userAgent;
};

// Forward remaining WebViewElement.foo* method calls to WebViewImpl.foo* or
// WebViewInternal.foo*.
forwardApiMethods(
    WebViewElement, WebViewImpl, WebViewInternal, WEB_VIEW_API_METHODS);

// Since |back| and |forward| are implemented in terms of |go|, we need to
// keep a reference to the real |go| function, since user code may override
// |WebViewElement.prototype.go|.
var originalGo = WebViewElement.prototype.go;

// Exports.
exports.$set('WebViewElement', WebViewElement);
