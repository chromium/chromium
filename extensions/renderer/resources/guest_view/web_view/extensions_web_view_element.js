// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The <webview> custom element. This defines <webview> at the extensions layer.

var forwardApiMethods = require('guestViewContainerElement').forwardApiMethods;
var registerElement = require('guestViewContainerElement').registerElement;
var WebViewElement = require('webViewElement').WebViewElement;
var WebViewImpl = require('webView').WebViewImpl;
var WebViewAttributeNames = require('webViewConstants').WebViewAttributeNames;
var WebViewInternal = getInternalApi('webViewInternal');
var WEB_VIEW_API_METHODS = require('webViewApiMethods').WEB_VIEW_API_METHODS;

class ExtensionsWebViewElement extends WebViewElement {
  static get observedAttributes() {
    return WebViewAttributeNames;
  }

  constructor() {
    super();
    privates(this).internal = new WebViewImpl(this);
    privates(this).originalGo = originalGo;
  }
}

// Forward remaining ExtensionsWebViewElement.foo* method calls to
// WebViewImpl.foo* or WebViewInternal.foo*. WebView APIs don't support
// promise-based syntax so |promiseMethodDetails| is left empty.
forwardApiMethods(
    ExtensionsWebViewElement, WebViewImpl, WebViewInternal,
    WEB_VIEW_API_METHODS, /*promiseMethodDetails=*/[]);

// Since |back| and |forward| are implemented in terms of |go|, we need to
// keep a reference to the real |go| function, since user code may override
// |ExtensionsWebViewElement.prototype.go|.
var originalGo = ExtensionsWebViewElement.prototype.go;

registerElement('WebView', ExtensionsWebViewElement);
