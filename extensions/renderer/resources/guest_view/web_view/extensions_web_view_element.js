// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The <webview> custom element. This defines <webview> at the extensions layer.

var registerElement = require('guestViewContainerElement').registerElement;
var WebViewElement = require('webViewElement').WebViewElement;
var WebViewImpl = require('webView').WebViewImpl;
var WebViewAttributeNames = require('webViewConstants').WebViewAttributeNames;

class ExtensionsWebViewElement extends WebViewElement {
  static get observedAttributes() {
    return WebViewAttributeNames;
  }

  constructor() {
    super();
    privates(this).internal = new WebViewImpl(this);
  }
}

registerElement('WebView', ExtensionsWebViewElement);
