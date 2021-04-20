// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Set of scripts required by web layer backed up by WKWebView.
goog.provide('__crWeb.mainFrameWebBundle');

// Requires __crWeb.form provided by __crWeb.allFramesWebBundle.

// DEPRECATED
// Do NOT add new features here, but rather add them using an instance of
// JavaScriptFeature. Please see the documentation at
// //ios/web/public/js_messaging/README.md
goog.require('__crWeb.navigation');
goog.require('__crWeb.textFragments');
