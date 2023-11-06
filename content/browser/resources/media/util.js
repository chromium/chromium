// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Some utility functions that don't belong anywhere else in the
 * code.
 */

/**
 * Calls a function for each element in an object/map/hash.
 *
 * @param obj The object to iterate over.
 * @param f The function to call on every value in the object.  F should have
 * the following arguments: f(value, key, object) where value is the value
 * of the property, key is the corresponding key, and obj is the object that
 * was passed in originally.
 * @param optObj The object use as 'this' within f.
 */
export function objectForEach(obj, f, optObj) {
  let key;
  for (key in obj) {
    if (obj.hasOwnProperty(key)) {
      f.call(optObj, obj[key], key, obj);
    }
  }
}

export function millisecondsToString(timeMillis) {
  function pad(num, len) {
    num = num.toString();
    while (num.length < len) {
      num = '0' + num;
    }
    return num;
  }

  const date = new Date(timeMillis);
  return pad(date.getUTCHours(), 2) + ':' + pad(date.getUTCMinutes(), 2) + ':' +
      pad(date.getUTCSeconds(), 2) + '.' +
      pad((date.getMilliseconds()) % 1000, 3);
}
