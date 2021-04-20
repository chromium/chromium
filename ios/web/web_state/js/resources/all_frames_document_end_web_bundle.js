// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Set of scripts injected into WKWebView at Document End.
goog.provide('__crWeb.allFramesDocumentEndWebBundle');

// DEPRECATED
// Do NOT add new features here, but rather add them using an instance of
// JavaScriptFeature. Please see the documentation at
// //ios/web/public/js_messaging/README.md
goog.require('__crWeb.setupFrame');
