// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/**
 * @fileoverview Preamble for JavaScript to be compiled with Closure Compiler.
 * We don't comple against the Closure library, so this provides a minimal set
 * of 'goog' namespace properties to support things like symbol exports.
 *
 * @provideGoog
 */

/** @const */
var goog = {};

/** @const */
goog.global = this;


/**
 * @param {string} name
 * @param {*} object
 * @param {Object=} opt_objectToExportTo
 */
goog.exportSymbol = function(name, object, opt_objectToExportTo) {
  let parts = name.split('.');
  let cur = opt_objectToExportTo || mojo.internal.globalScope;
  for (let part; parts.length && (part = parts.shift());) {
    if (!parts.length && object !== undefined)
      cur[part] = object;
    else if (cur[part] && cur[part] != Object.prototype[part])
      cur = cur[part];
    else
      cur = cur[part] = {};
  }
};


/**
 * @param {Object} object
 * @param {string} publicName
 * @param {*} symbol
 */
goog.exportProperty = function(object, publicName, symbol) {
  object[publicName] = symbol;
};
