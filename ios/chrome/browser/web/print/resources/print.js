// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file adheres to closure-compiler conventions in order to enable
// compilation with ADVANCED_OPTIMIZATIONS. In particular, members that are to
// be accessed externally should be specified in this['style'] as opposed to
// this.style because member identifiers are minified by default.
// See http://goo.gl/FwOgy

// Intercept window.print calls.

goog.provide('__crWeb.print');

/**
 * Namespace for this module.
 */
__gCrWeb['print'] = {};

new function() {
  // Overwrites window.print function to invoke chrome command.
  window.print = function() {
    __gCrWeb.common.sendWebKitMessage('PrintMessageHandler', {});
  };
}
