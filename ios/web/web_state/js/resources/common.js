// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides common methods that can be shared by other JavaScripts.

// Requires functions from base.js.

/** @typedef {HTMLInputElement|HTMLTextAreaElement|HTMLSelectElement} */
var FormControlElement;

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
  if (typeof(value.toJSON) == 'function') {
    // Prevents websites from changing stringify's behavior by adding the
    // method toJSON() by temporarily removing it.
    var originalToJSON = value.toJSON;
    value.toJSON = undefined;
    var stringifiedValue = __gCrWeb.common.JSONStringify(value);
    value.toJSON = originalToJSON;
    return stringifiedValue;
  }
  return __gCrWeb.common.JSONStringify(value);
};

/**
 * Returns if an element is a text field.
 * This returns true for all of textfield-looking types such as text,
 * password, search, email, url, and number.
 *
 * This method aims to provide the same logic as method
 *     bool WebInputElement::isTextField() const
 * in chromium/src/third_party/WebKit/Source/WebKit/chromium/src/
 * WebInputElement.cpp, where this information is from
 *     bool HTMLInputElement::isTextField() const
 *     {
 *       return m_inputType->isTextField();
 *     }
 * (chromium/src/third_party/WebKit/Source/WebCore/html/HTMLInputElement.cpp)
 *
 * The implementation here is based on the following:
 *
 * - Method bool InputType::isTextField() defaults to be false and it is
 *   override to return true only in HTMLInputElement's subclass
 *   TextFieldInputType (chromium/src/third_party/WebKit/Source/WebCore/html/
 *   TextFieldInputType.h).
 *
 * - The implementation here considers all the subclasses of
 *   TextFieldInputType: NumberInputType and BaseTextInputType, which has
 *   subclasses EmailInputType, PasswordInputType, SearchInputType,
 *   TelephoneInputType, TextInputType, URLInputType. (All these classes are
 *   defined in chromium/src/third_party/WebKit/Source/WebCore/html/)
 *
 * @param {Element} element An element to examine if it is a text field.
 * @return {boolean} true if element has type=text.
 */
__gCrWeb.common.isTextField = function(element) {
  if (!element) {
    return false;
  }
  if (element.type === 'hidden') {
    return false;
  }
  return element.type === 'text' || element.type === 'email' ||
      element.type === 'password' || element.type === 'search' ||
      element.type === 'tel' || element.type === 'url' ||
      element.type === 'number';
};

/**
 * Trims any whitespace from the start and end of a string.
 * Used in preference to String.prototype.trim as this can be overridden by
 * sites.
 *
 * @param {string} str The string to be trimmed.
 * @return {string} The string after trimming.
 */
__gCrWeb.common.trim = function(str) {
  return str.replace(/^\s+|\s+$/g, '');
};

/**
 * Extracts the webpage URL from the given URL by removing the query
 * and the reference (aka fragment) from the URL.
 *
 * IMPORTANT: Not security proof, do not assume the URL returns by this
 * function reflects what is actually on the page as the hosted page can
 * modify the behavior of the window.URL prototype.
 *
 * @param {string} url Web page URL.
 * @return {string} Web page URL with query and reference removed. An empty
 *   string if the window.URL prototype was changed by the hosted page.
 */
__gCrWeb.common.removeQueryAndReferenceFromURL = function(url) {
  var parsed = new URL(url);

  const isPropertyInvalid = (value) => typeof value !== 'string';

  if (isPropertyInvalid(parsed.origin) || isPropertyInvalid(parsed.protocol) ||
      isPropertyInvalid(parsed.pathname)) {
    // If at least one of these properties is not of a string type, it is a sign
    // that the window.URL prototype was changed by the hosted page in the page
    // content world. Return an empty string in that case as URL has an
    // undefined behavior. This doesn't cover all window.URL mutations, but it
    // at least shields against getting non-string values from these
    // properties. The returned URL will be malformed in the worst case but is
    // guaranteed to be a string.
    return '';
  }

  // For some protocols (eg. data:, javascript:) URL.origin is "null" string
  // (not the type) so URL.protocol is used instead.
  return (parsed.origin !== 'null' ? parsed.origin : parsed.protocol) +
      parsed.pathname;
};

/**
 * Posts |message| to the webkit message handler specified by |handlerName|.
 * DEPRECATED: This function will be removed soon. Instead, use the
 * implementation at //ios/web/public/js_messaging/resources/utils.ts
 *
 * @param {string} handlerName The name of the webkit message handler.
 * @param {Object} message The message to post to the handler.
 */
__gCrWeb.common.sendWebKitMessage = function(handlerName, message) {
  try {
    // A web page can override `window.webkit` with any value. Deleting the
    // object ensures that original and working implementation of
    // window.webkit is restored.
    var oldWebkit = window.webkit;
    delete window['webkit'];
    window.webkit.messageHandlers[handlerName].postMessage(message);
    window.webkit = oldWebkit;
  } catch (err) {
    // TODO(crbug.com/40269960): Report this fatal error
  }
};
