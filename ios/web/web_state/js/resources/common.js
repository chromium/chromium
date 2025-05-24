// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides common methods that can be shared by other JavaScripts.

// Requires functions from base.js.

/** @typedef {HTMLInputElement|HTMLTextAreaElement|HTMLSelectElement} */
let FormControlElement;

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected. String 'common' is used in |__gCrWeb['common']| as it needs to be
 * accessed in Objective-C code.
 */
__gCrWeb.common = {};

// Store common namespace object in a global __gCrWeb object referenced by a
// string, so it does not get renamed by closure compiler during the
// minification.
__gCrWeb['common'] = __gCrWeb.common;

/**
 * JSON safe object to protect against custom implementation of Object.toJSON
 * in host pages.
 * @constructor
 */
__gCrWeb.common.JSONSafeObject = function JSONSafeObject() {};

/**
 * Protect against custom implementation of Object.toJSON in host pages.
 */
__gCrWeb.common.JSONSafeObject.prototype['toJSON'] = null;

/**
 * Retain the original JSON.stringify method where possible to reduce the
 * impact of sites overriding it
 */
__gCrWeb.common.JSONStringify = JSON.stringify;

/**
 * Returns a string that is formatted according to the JSON syntax rules.
 * This is equivalent to the built-in JSON.stringify() function, but is
 * less likely to be overridden by the website itself.  Prefer the private
 * {@code __gcrWeb.common.JSONStringify} whenever possible instead of using
 * this public function. The |__gCrWeb| object itself does not use it; it uses
 * its private counterpart instead.
 * @param {*} value The value to convert to JSON.
 * @return {string} The JSON representation of value.
 */
__gCrWeb.stringify = function(value) {
  if (value === null) return 'null';
  if (value === undefined) return 'undefined';
  if (typeof (value.toJSON) === 'function') {
    // Prevents websites from changing stringify's behavior by adding the
    // method toJSON() by temporarily removing it.
    const originalToJSON = value.toJSON;
    value.toJSON = undefined;
    const stringifiedValue = __gCrWeb.common.JSONStringify(value);
    value.toJSON = originalToJSON;
    return stringifiedValue;
  }
  return __gCrWeb.common.JSONStringify(value);
};
