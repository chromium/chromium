// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The set of scripts to be injected into the main frame of the web view as
// early as possible.
goog.provide('__crWeb.webViewMainFrame');

goog.require('__crWeb.languageDetection');
// password_controller.js requires migration into new js injection API before
// this line can be moved into web_view_all_frames.js.
goog.require('__crWeb.passwords');
