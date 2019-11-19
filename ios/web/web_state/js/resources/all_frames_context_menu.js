// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview APIs used by CRWContextMenuController.
 */

goog.provide('__crWeb.allFramesContextMenu');

goog.require('__crWeb.base');
goog.require('__crWeb.common');

/** Beginning of anonymous object */
(function() {

/**
 * Returns an object representing the details of a given link element.
 * @param {HTMLElement} element The element whose details will be returned.
 * @return {!Object} An object of the form {
 *                     {@code href} The URL of the link.
 *                     {@code referrerPolicy} The referrer policy to use for
 *                         navigations away from the current page.
 *                     {@code innerText} The inner text of the link.
 *                   }.
 */
var getResponseForLinkElement = function(element) {
  return {
    href: getElementHref_(element),
    referrerPolicy: getReferrerPolicy_(element),
    innerText: element.innerText
  };
};

/**
 * Returns an object representing the details of a given image element.
 * @param {HTMLElement} element The element whose details will be returned.
 * @return {!Object} An object of the form {
 *                     {@code src} The src of the image.
 *                     {@code referrerPolicy} The referrer policy to use for
 *                         navigations away from the current page.
 *                     {@code title} (optional) The title of the image, if one
 *                         exists.
 *                     {@code href} (optional) The URL of the link, if one
 *                         exists.
 *                   }.
 */
var getResponseForImageElement = function(element) {
  var result = {src: element.src, referrerPolicy: getReferrerPolicy_()};
  var parent = element.parentNode;
  // Copy the title, if any.
  if (element.title) {
    result.title = element.title;
  }
  // Check if the image is also a link.
  while (parent) {
    if (parent.tagName && parent.tagName.toLowerCase() === 'a' &&
        parent.href) {
      // This regex identifies strings like void(0),
      // void(0)  ;void(0);, ;;;;
      // which result in a NOP when executed as JavaScript.
      var regex = RegExp('^javascript:(?:(?:void\\(0\\)|;)\\s*)+$');
      if (parent.href.match(regex)) {
        parent = parent.parentNode;
        continue;
      }
      result.href = parent.href;
      result.referrerPolicy = getReferrerPolicy_(parent);
      break;
    }
  parent = parent.parentNode;
  }
  return result;
};

/**
 * Finds the url of the image or link under the selected point. Sends details
 * about the found element (or an empty object if no links or images are found)
 * back to the application by posting a 'FindElementResultHandler' message.
 * The object will be of the same form as returned by
 * {@code getResponseForLinkElement} or {@code getResponseForImageElement}.
 * @param {string} requestId An identifier which will be returned in the result
 *                 dictionary of this request.
 * @param {number} x Horizontal center of the selected point in page
 *                 coordinates.
 * @param {number} y Vertical center of the selected point in page
 *                 coordinates.
 */
__gCrWeb['findElementAtPointInPageCoordinates'] = function(requestId, x, y) {
  var hitCoordinates = spiralCoordinates_(x, y);
  for (var index = 0; index < hitCoordinates.length; index++) {
    var coordinates = hitCoordinates[index];

    var coordinateDetails = newCoordinate(coordinates.x, coordinates.y);
    var element = elementsFromCoordinates(window.document, coordinateDetails);
    // if element is a frame, tell it to respond to this element request
    if (element &&
        (element.tagName.toLowerCase() === 'iframe' ||
         element.tagName.toLowerCase() === 'frame')) {
      var payload = {
        type: 'org.chromium.contextMenuMessage',
        requestId: requestId,
        x: x - element.offsetLeft,
        y: y - element.offsetTop
      };
      // The message will not be sent if |targetOrigin| is null, so use * which
      // allows the message to be delievered to the contentWindow regardless of
      // the origin.
      var targetOrigin = element.src || '*';
      element.contentWindow.postMessage(payload, targetOrigin);
      return;
    }

    if (!element || !element.tagName) {
      // Nothing under the hit point. Try the next hit point.
      continue;
    }

    // Also check element's ancestors. A bound on the level is used here to
    // avoid large overhead when no links or images are found.
    var level = 0;
    while (++level < 8 && element && element != document) {
      var tagName = element.tagName;
      if (!tagName) continue;
      tagName = tagName.toLowerCase();

      if (tagName === 'input' || tagName === 'textarea' ||
          tagName === 'select' || tagName === 'option') {
        // If the element is a known input element, stop the spiral search and
        // return empty results.
        sendFindElementAtPointResponse(requestId, /*response=*/{});
        return;
      }

      if (getComputedWebkitTouchCallout_(element) !== 'none') {
        if (tagName === 'a' && element.href) {
          sendFindElementAtPointResponse(requestId,
                                         getResponseForLinkElement(element));
          return;
        }

        if (tagName === 'img' && element.src) {
          sendFindElementAtPointResponse(requestId,
                                         getResponseForImageElement(element));
          return;
        }
      }
      element = element.parentNode;
    }
  }
  sendFindElementAtPointResponse(requestId, /*response=*/{});
};

/**
 * Inserts |requestId| into |response| and sends the result as the payload of a
 * 'FindElementResultHandler' message back to the native application.
 * @param {string} requestId An identifier which will be returned in the result
 *                 dictionary of this request.
 * @param {!Object} response The 'FindElementResultHandler' message payload.
 */
var sendFindElementAtPointResponse = function(requestId, response) {
  response.requestId = requestId;
  __gCrWeb.common.sendWebKitMessage('FindElementResultHandler', response);
};

/**
 * Returns whether or not view port coordinates should be used for the given
 * window.
 * @return {boolean} True if the window has been scrolled down or to the right,
 *                   false otherwise.
 */
var elementFromPointIsUsingViewPortCoordinates = function(win) {
  if (win.pageYOffset > 0) {  // Page scrolled down.
    return (
        win.document.elementFromPoint(
            0, win.pageYOffset + win.innerHeight - 1) === null);
  }
  if (win.pageXOffset > 0) {  // Page scrolled to the right.
    return (
        win.document.elementFromPoint(
            win.pageXOffset + win.innerWidth - 1, 0) === null);
  }
  return false;  // No scrolling, don't care.
};

/**
 * Returns the coordinates of the upper left corner of |obj| in the
 * coordinates of the window that |obj| is in.
 * @param {HTMLElement} el The element whose coordinates will be returned.
 * @return {!Object} coordinates of the given object.
 */
var getPositionInWindow = function(el) {
  var coord = {x: 0, y: 0};
  while (el.offsetParent) {
    coord.x += el.offsetLeft;
    coord.y += el.offsetTop;
    el = el.offsetParent;
  }
  return coord;
};

/**
 * Returns details about a given coordinate in {@code window}.
 * @param {number} x The x component of the coordinate in {@code window}.
 * @param {number} y The y component of the coordinate in {@code window}.
 * @return {!Object} Details about the given coordinate and the current window.
 */
var newCoordinate = function(x, y) {
  var coordinates = {
    x: x,
    y: y,
    viewPortX: x - window.pageXOffset,
    viewPortY: y - window.pageYOffset,
    useViewPortCoordinates: false,
    window: window
  };
  return coordinates;
};

/**
 * Returns the element at the given coordinates.
 * @param {Object} root The Document or ShadowRoot object to search within.
 * @param {Object} coordinates Page coordinates in the same format as the result
 *                             from {@code newCoordinate}.
 */
var elementsFromCoordinates = function(root, coordinates) {
  coordinates.useViewPortCoordinates = coordinates.useViewPortCoordinates ||
      elementFromPointIsUsingViewPortCoordinates(coordinates.window);

  var currentElement = null;
  if (coordinates.useViewPortCoordinates) {
    currentElement = root.elementFromPoint(
        coordinates.viewPortX, coordinates.viewPortY);
  } else {
    currentElement = root.elementFromPoint(coordinates.x, coordinates.y);
  }

  // Check for tagName, because if a selection is made by the WebView, the
  // element we will get won't have one.
  if (!currentElement || !currentElement.tagName) {
    return null;
  }

  if (currentElement.tagName.toLowerCase() === 'iframe' ||
      currentElement.tagName.toLowerCase() === 'frame') {
    // Check if the frame is in a different domain using only information
    // visible to the current frame (i.e. currentElement.src) to avoid
    // triggering a SecurityError in the console.
    if (!__gCrWeb.common.isSameOrigin(
        window.location.href, currentElement.src)) {
      return currentElement;
    }
    var framePosition = getPositionInWindow(currentElement);
    coordinates.viewPortX -= framePosition.x - coordinates.window.pageXOffset;
    coordinates.viewPortY -= framePosition.y - coordinates.window.pageYOffset;
    coordinates.window = currentElement.contentWindow;
    coordinates.x -= framePosition.x + coordinates.window.pageXOffset;
    coordinates.y -= framePosition.y + coordinates.window.pageYOffset;
    return elementsFromCoordinates(coordinates.window.document, coordinates);
  }

  if (currentElement.shadowRoot) {
    // The element's shadowRoot can be the same as |root| if the point is not
    // on any child element. Break the recursion and return no found element.
    if (currentElement.shadowRoot == root) {
      return null;
    }
    return elementsFromCoordinates(currentElement.shadowRoot, coordinates);
  }
  return currentElement;
};

/** @private
 * @param {number} x
 * @param {number} y
 * @return {Object}
 */
var spiralCoordinates_ = function(x, y) {
  var MAX_ANGLE = Math.PI * 2.0 * 2.0;
  var POINT_COUNT = 10;
  var ANGLE_STEP = MAX_ANGLE / POINT_COUNT;
  var TOUCH_MARGIN = 15;
  var SPEED = TOUCH_MARGIN / MAX_ANGLE;

  var coordinates = [];
  for (var index = 0; index < POINT_COUNT; index++) {
    var angle = ANGLE_STEP * index;
    var radius = angle * SPEED;

    coordinates.push({
      x: x + Math.round(Math.cos(angle) * radius),
      y: y + Math.round(Math.sin(angle) * radius)
    });
  }

  return coordinates;
};

/** @private
 * @param {HTMLElement} element
 * @return {Object}
 */
var getComputedWebkitTouchCallout_ = function(element) {
  return window.getComputedStyle(element, null)['webkitTouchCallout'];
};

/**
 * Gets the referrer policy to use for navigations away from the current page.
 * If a link element is passed, and it includes a rel=noreferrer tag, that
 * will override the page setting.
 * @param {HTMLElement=} opt_linkElement The link triggering the navigation.
 * @return {string} The policy string.
 * @private
 */
var getReferrerPolicy_ = function(opt_linkElement) {
  if (opt_linkElement) {
    var rel = opt_linkElement.getAttribute('rel');
    if (rel && rel.toLowerCase() == 'noreferrer') {
      return 'never';
    }
  }

  // Search for referrer meta tag.  WKWebView only supports a subset of values
  // for referrer meta tags.  If it parses a referrer meta tag with an
  // unsupported value, it will default to 'never'.
  var metaTags = document.getElementsByTagName('meta');
  for (var i = 0; i < metaTags.length; ++i) {
    if (metaTags[i].name.toLowerCase() == 'referrer') {
      var referrerPolicy = metaTags[i].content.toLowerCase();
      if (referrerPolicy == 'default' || referrerPolicy == 'always' ||
          referrerPolicy == 'no-referrer' || referrerPolicy == 'origin' ||
          referrerPolicy == 'no-referrer-when-downgrade' ||
          referrerPolicy == 'unsafe-url') {
        return referrerPolicy;
      } else {
        return 'never';
      }
    }
  }
  return 'default';
};

 /**
  * Returns the href of the given element. Handles standard <a> links as well as
  * xlink:href links as used within <svg> tags.
  * @param {HTMLElement} element The link triggering the navigation.
  * @return {string} The href of the given element.
  * @private
  */
var getElementHref_ = function(element) {
  var href = element.href;
  if (href instanceof SVGAnimatedString) {
    return href.animVal
  }
  return href
};

/**
 * Processes context menu messages received by the window.
 */
window.addEventListener('message', function(message) {
  var payload = message.data;
  if (payload.hasOwnProperty('type') &&
      payload.type == 'org.chromium.contextMenuMessage') {
    __gCrWeb.findElementAtPointInPageCoordinates(payload.requestId,
                                                 payload.x,
                                                 payload.y);
  }
});

}());  // End of anonymouse object
