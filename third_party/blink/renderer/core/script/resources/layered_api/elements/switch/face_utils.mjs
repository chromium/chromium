// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @file Utilities for form-associated custom elements
 */

import * as reflection from '../internal/reflection.mjs';

function installGetter(proto, propName, getter) {
  Object.defineProperty(
      getter, 'name',
      {configurable: true, enumerable: false, value: 'get ' + propName});
  Object.defineProperty(
      proto, propName, {configurable: true, enumerable: true, get: getter});
}

/**
 * Add the following properties to |proto|
 *   - disabled
 *   - name
 *   - type
 * and make the following properties and functions enumerable
 *   - form
 *   - willValidate
 *   - validity
 *   - validationMessage
 *   - labels
 *   - checkValidity()
 *   - reportValidity()
 *   - setCustomValidity(error)
 *
 * @param {!Object} proto An Element prototype which will have properties
 */
export function installProperties(proto) {
  reflection.installBool(proto, 'disabled');
  reflection.installString(proto, 'name');
  installGetter(proto, 'type', function() {
    if (!(this instanceof proto.constructor)) {
      throw new TypeError(
          'The context object is not an instance of ' + proto.contructor.name);
    }
    return this.localName;
  });

  Object.defineProperty(proto, 'form', {enumerable: true});
  Object.defineProperty(proto, 'willValidate', {enumerable: true});
  Object.defineProperty(proto, 'validity', {enumerable: true});
  Object.defineProperty(proto, 'validationMessage', {enumerable: true});
  Object.defineProperty(proto, 'labels', {enumerable: true});

  Object.defineProperty(proto, 'checkValidity', {enumerable: true});
  Object.defineProperty(proto, 'reportValidity', {enumerable: true});
  Object.defineProperty(proto, 'setCustomValidity', {enumerable: true});
}
