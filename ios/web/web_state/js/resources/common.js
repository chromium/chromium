// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides common methods that can be shared by other JavaScripts.

goog.provide('__crWeb.common');

goog.require('__crWeb.base');

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

/* Beginning of anonymous object. */
(function() {

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
 * Tests an element's visiblity. This test is expensive so should be used
 * sparingly.
 * @param {Element} element A DOM element.
 * @return {boolean} true if the |element| is currently part of the visible
 * DOM.
 */
__gCrWeb.common.isElementVisible = function(element) {
  /** @type {Node} */
  var node = element;
  while (node && node !== document) {
    if (node.nodeType === Node.ELEMENT_NODE) {
      var style = window.getComputedStyle(/** @type {Element} */ (node));
      if (style.display === 'none' || style.visibility === 'hidden') {
        return false;
      }
    }
    // Move up the tree and test again.
    node = node.parentNode;
  }
  // Test reached the top of the DOM without finding a concealed ancestor.
  return true;
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
 * Acquires the specified DOM |attribute| from the DOM |element| and returns
 * its lower-case value, or null if not present.
 * @param {Element} element A DOM element.
 * @param {string} attribute An attribute name.
 * @return {?string} Lowercase value of DOM element or null if not present.
 */
__gCrWeb.common.getLowerCaseAttribute = function(element, attribute) {
  if (!element) {
    return null;
  }
  var value = element.getAttribute(attribute);
  if (value) {
    return value.toLowerCase();
  }
  return null;
};

/**
 * Converts a relative URL into an absolute URL.
 * @param {Object} doc Document.
 * @param {string} relativeURL Relative URL.
 * @return {string} Absolute URL.
 */
__gCrWeb.common.absoluteURL = function(doc, relativeURL) {
  // In the case of data: URL-based pages, relativeURL === absoluteURL.
  if (doc.location.protocol === 'data:') {
    return doc.location.href;
  }
  var urlNormalizer = doc['__gCrWebURLNormalizer'];
  if (!urlNormalizer) {
    urlNormalizer = doc.createElement('a');
    doc['__gCrWebURLNormalizer'] = urlNormalizer;
  }

  // Use the magical quality of the <a> element. It automatically converts
  // relative URLs into absolute ones.
  urlNormalizer.href = relativeURL;
  return urlNormalizer.href;
};

/**
 * Extracts the webpage URL from the given URL by removing the query
 * and the reference (aka fragment) from the URL.
 * @param {string} url Web page URL.
 * @return {string} Web page URL with query and reference removed.
 */
__gCrWeb.common.removeQueryAndReferenceFromURL = function(url) {
  var parsed = new URL(url);
  // For some protocols (eg. data:, javascript:) URL.origin is "null" so
  // URL.protocol is used instead.
  return (parsed.origin !== 'null' ? parsed.origin : parsed.protocol) +
      parsed.pathname;
};

/**
 * Retrieves favicon information.
 *
 * @return {Object} Object containing favicon data.
 */
__gCrWeb.common.getFavicons = function() {
  var favicons = [];
  delete favicons.toJSON;  // Never inherit Array.prototype.toJSON.
  var links = document.getElementsByTagName('link');
  var linkCount = links.length;
  for (var i = 0; i < linkCount; ++i) {
    if (links[i].rel) {
      var rel = links[i].rel.toLowerCase();
      if (rel == 'shortcut icon' || rel == 'icon' ||
          rel == 'apple-touch-icon' || rel == 'apple-touch-icon-precomposed') {
        var favicon = {rel: links[i].rel.toLowerCase(), href: links[i].href};
        if (links[i].sizes && links[i].sizes.value) {
          favicon.sizes = links[i].sizes.value;
        }
        favicons.push(favicon);
      }
    }
  }
  return favicons;
};

/**
 * Checks whether the two URLs are from the same origin.
 * @param {string} url_one
 * @param {string} url_two
 * @return {boolean} Whether the two URLs have the same origin.
 */
__gCrWeb.common.isSameOrigin = function(url_one, url_two) {
  if (!url_one || !url_two) {
    // Attempting to create URL representations of an empty string throws an
    // exception.
    return false;
  }
  return new URL(url_one).origin == new URL(url_two).origin;
};

/**
 * Posts |message| to the webkit message handler specified by |handlerName|.
 *
 * @param {string} handlerName The name of the webkit message handler.
 * @param {Object} message The message to post to the handler.
 */
__gCrWeb.common.sendWebKitMessage = function(handlerName, message) {
  // A web page can override |window.webkit| with any value. Deleting the
  // object ensures that original and working implementation of
  // window.webkit is restored.
  var oldWebkit = window.webkit;
  delete window['webkit'];
  window.webkit.messageHandlers[handlerName].postMessage(message);
  window.webkit = oldWebkit;
};

}());  // End of anonymous object
