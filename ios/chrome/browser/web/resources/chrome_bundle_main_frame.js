// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The set of scripts to be injected into the web view as early as possible.
goog.provide('__crWeb.chromeBundleMainFrame');

// DEPRECATED
// Do NOT add new features here, but rather add them using an instance of
// JavaScriptFeature. Please see the documentation at
// //ios/web/public/js_messaging/README.md
goog.require('__crWeb.languageDetection');
