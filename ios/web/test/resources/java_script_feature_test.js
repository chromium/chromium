// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Add functionality used in JavaScriptFeature inttests.
 */
goog.provide('__crWeb.javaScriptFeatureTest');

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected.
 */
__gCrWeb.javaScriptFeatureTest = {};

// Store namespace object in a global __gCrWeb object referenced by a
// string, so it does not get renamed by closure compiler during the
// minification.
__gCrWeb['javaScriptFeatureTest'] = __gCrWeb.javaScriptFeatureTest;

__gCrWeb.javaScriptFeatureTest.replaceDivContents = function() {
  document.getElementById('div').innerHTML = 'updated';
};

document.getElementsByTagName("body")[0].appendChild(
  document.createTextNode("injected_script_loaded")
);
