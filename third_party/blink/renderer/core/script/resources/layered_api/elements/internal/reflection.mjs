// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @file Manage attribute-property reflections.
 *     https://html.spec.whatwg.org/C/common-dom-interfaces.html#reflecting-content-attributes-in-idl-attributes
 */

/**
 * Add a bool reflection property to the specified prototype for the specified
 * attribute.
 *
 * @param {!Object} proto An element prototype
 * @param {string} attrName An attribute name
 * @param {string} propName An optional property name. attrName will be used if
 *     this argument is omitted.
 */
export function installBool(proto, attrName, propName = attrName) {
  function getter() {
    return this.hasAttribute(attrName);
  }
  function setter(value) {
    this.toggleAttribute(attrName, Boolean(value));
  }
  Object.defineProperty(
      getter, 'name',
      {configurable: true, enumerable: false, value: 'get ' + propName});
  Object.defineProperty(
      setter, 'name',
      {configurable: true, enumerable: false, value: 'set ' + propName});
  Object.defineProperty(
      proto, propName,
      {configurable: true, enumerable: true, get: getter, set: setter});
}

/**
 * Add a DOMString reflection property to the specified prototype for the
 * specified attribute.
 *
 * @param {!Element} element An element prototype
 * @param {string} attrName An attribute name
 * @param {string} propName An optional property name. attrName will be used if
 *     this argument is omitted.
 */
export function installString(proto, attrName, propName = attrName) {
  function getter() {
    const value = this.getAttribute(attrName);
    return value === null ? '' : value;
  }
  function setter(value) {
    this.setAttribute(attrName, value);
  }
  Object.defineProperty(
      getter, 'name',
      {configurable: true, enumerable: false, value: 'get ' + propName});
  Object.defineProperty(
      setter, 'name',
      {configurable: true, enumerable: false, value: 'set ' + propName});
  Object.defineProperty(
      proto, propName,
      {configurable: true, enumerable: true, get: getter, set: setter});
}
