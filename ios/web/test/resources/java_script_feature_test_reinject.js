// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Setup used in JavaScriptFeature inttests. This file
 * will be reinjected if the document JS object is modified.
 */
goog.provide('__crWeb.javaScriptFeatureTestReinject');

window.addEventListener('error', function(event) {
  __gCrWeb.javaScriptFeatureTest.errorReceivedCount =
      __gCrWeb.javaScriptFeatureTest.errorReceivedCount + 1;
});
