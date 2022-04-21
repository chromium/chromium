// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file adheres to closure-compiler conventions in order to enable
// compilation with ADVANCED_OPTIMIZATIONS. In particular, members that are to
// be accessed externally should be specified in this['style'] as opposed to
// this.style because member identifiers are minified by default.
// See http://goo.gl/FwOgy

var goog;
goog.provide('__crWeb.base');

// This object is checked on the main app to know when to inject (or not).
var __gCrWeb = {};

// Store __gCrWeb global namespace object referenced by a string, so it does not
// get renamed by closure compiler during the minification.
window['__gCrWeb'] = __gCrWeb;
