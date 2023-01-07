// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview APIs used by CRWContextMenuController.
 */

// Requires functions from base.js and common.js

import {getSurroundingText} from '//ios/web/js_features/context_menu/resources/surrounding_text.js';

// The minimum opacity for an element to be considered as opaque. Elements
// with a higher opacity will prevent selection of images underneath.
var OPACITY_THRESHOLD = 0.9;

// The maximum opacity for an element to be considered as transparent.
// Images with a lower opacity will not be selectable.
var TRANSPARENCY_THRESHOLD = 0.1;

// The maximum depth to search for elements at any point.
var MAX_SEARCH_DEPTH = 8;

/**
 * Returns an object representing the details of a given link element.
 * @param {HTMLElement} element The element whose details will be returned.
 * @return {!Object} An object of the form {
 *                     {@code tagName} 'a'
 *                     {@code href} The URL of the link.
 *                     {@code referrerPolicy} The referrer policy to use for
 *                         navigations away from the current page.
 *                     {@code innerText} The inner text of the link.
 *                   }.
 */
var getResponseForLinkElement = function(element) {
  return {
    tagName: 'a',
    href: getElementHref_(element),
    referrerPolicy: getReferrerPolicy_(element),
    innerText: element.innerText,
  };
};

/**
 * Returns an object representing the details of a given image element.
 * @param {HTMLElement} element The element whose details will be returned.
 * @param {string} src The source of the image or image-like element.
 * @return {!Object} An object of the form {
 *                     {@code tagName} 'img'
 *                     {@code src} The src of the image.
 *                     {@code referrerPolicy} The referrer policy to use for
 *                         navigations away from the current page.
 *                     {@code title} (optional) The title of the image, if one
 *                         exists.
 *                     {@code href} (optional) The URL of the link, if one
 *                         exists.
 *                   }.
 */
var getResponseForImageElement = function(element, src) {
  var result = {
    tagName: 'img',
    src: src,
    referrerPolicy: getReferrerPolicy_(),
  };
  var parent = element.parentNode;
  // Copy the title, if any.
  if (element.title) {
    result.title = element.title;
  }
  // Copy the alt text attribute, if any.
  if (element.alt) {
    result.alt = element.alt;
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
 * Returns an object representing the details of a given text element.
 * @param {HTMLElement} element The element whose details will be returned.
 * @param {number} x Horizontal center of the selected point in page
 *                 coordinates.
 * @param {number} y Vertical center of the selected point in page
 *                 coordinates.
 * @param {bool} Enables getting the surrounding characters if
 *               true.
 * @return {!Object} An object of the form {
 *                     {@code tagName} tag name of the text element.
 *                     {@code innerText} The inner text of the link.
 *                     {@code textOffset} The tap character offset in
 *                         innertText.
 *                   }.
 */
var getResponseForTextElement = function(
    element, x, y, extractSurroundingText) {
  var result = {
    tagName: element.tagName,
  };

  // caretRangeFromPoint is custom WebKit method.
  if (document.caretRangeFromPoint) {
    var range = document.caretRangeFromPoint(x, y);
    if (range && range.startContainer) {
      var textNode = range.startContainer;
      if (textNode.nodeType == 3) {
        result.textOffset = range.startOffset;
        result.innerText = textNode.nodeValue;

        if (extractSurroundingText) {
          var textAndStartPos = getSurroundingText(range);
          result.surroundingText = textAndStartPos['text'];
          result.surroundingTextOffset = textAndStartPos['pos'];
        }
      }
    }
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
 * @param {bool} Enables getting the surrounding characters if
 *               true.
 */
__gCrWeb['findElementAtPointInPageCoordinates'] = function(
    requestId, x, y, extractSurroundingText) {
  var hitCoordinates = spiralCoordinates_(x, y);
  var processedElements = new Set();
  var firstDefaultElement = [];
  for (var index = 0; index < hitCoordinates.length; index++) {
    var coordinates = hitCoordinates[index];

    var coordinateDetails = newCoordinate(coordinates.x, coordinates.y);
    coordinateDetails.useViewPortCoordinates =
        coordinateDetails.useViewPortCoordinates ||
        elementFromPointIsUsingViewPortCoordinates(coordinateDetails.window);

    var coordinateX = coordinateDetails.useViewPortCoordinates ?
        coordinateDetails.viewPortX :
        coordinateDetails.x;
    var coordinateY = coordinateDetails.useViewPortCoordinates ?
        coordinateDetails.viewPortY :
        coordinateDetails.y;
    var elementWasFound = findElementAtPoint(
        requestId, window.document, processedElements, coordinateX, coordinateY,
        x, y, firstDefaultElement);

    // Exit early if an element was found.
    if (elementWasFound) {
      return;
    }
  }

  if (firstDefaultElement.length > 0) {
    sendFindElementAtPointResponse(
        requestId,
        getResponseForTextElement(
            firstDefaultElement[0], x - window.pageXOffset,
            y - window.pageYOffset, extractSurroundingText));
    return;
  }

  // If no element was found, send an empty response.
  sendFindElementAtPointResponse(requestId, /*response=*/ {});
};

/**
 * Finds the topmost valid element at the given point.
 * @param {string} requestId An identifier which will be returned in the result
 *                 dictionary of this request.
 * @param {Object} root The Document or ShadowRoot object to search within.
 * @param {Object} processedElements A set to store processed elements in.
 * @param {number} pointX The X coordinate of the target location.
 * @param {number} pointY The Y coordinate of the target location.
 * @param {number} centerX The X coordinate of the center of the target.
 * @param {number} centerY The Y coordinate of the center of the target.
 * @return {boolean} Whether or not an element was matched as a target of
 *                   the touch.
 */
var findElementAtPoint = function(
    requestId, root, processedElements, pointX, pointY, centerX, centerY,
    firstDefaultElement) {
  var elements = root.elementsFromPoint(pointX, pointY);
  var foundLinkElement;
  for (var elementIndex = 0;
       elementIndex < elements.length && elementIndex < MAX_SEARCH_DEPTH;
       elementIndex++) {
    var element = elements[elementIndex];

    // Element.closest will find link elements that are parents of the current
    // element. It also works for SVGAElements, links within svg tags. However,
    // we must still iterate through the elements at this position to find
    // images. This ensures that it will never miss the link, even if this loop
    //  terminates due to hitting an opaque element.
    if (!foundLinkElement) {
      var closestLink = element.closest('a');
      if (closestLink && closestLink.href &&
          getComputedWebkitTouchCallout_(closestLink) !== 'none') {
        foundLinkElement = closestLink;
      }
    }

    if (!processedElements.has(element)) {
      processedElements.add(element);
      if (element.shadowRoot) {
        // The element's shadowRoot can be the same as |root| if the point is
        // not on any child element. Break the recursion and return no found
        // element.
        if (element.shadowRoot == root) {
          return false;
        }

        // If an element is found in the shadow DOM, return true, otherwise
        // keep iterating.
        if (findElementAtPoint(
                requestId, element.shadowRoot, processedElements, pointX,
                pointY, centerX, centerY, firstDefaultElement)) {
          return true;
        }
      }

      if (processElementForFindElementAtPoint(
              requestId, centerX, centerY, element)) {
        return true;
      }

      if (element.tagName != 'HTML' &&
          element.tagName != 'IMG' &&
          element.tagName != 'svg' &&
          firstDefaultElement.length == 0) {
        firstDefaultElement.push(element);
      }
    }

    // Opaque elements should block taps on images that are behind them.
    if (isOpaqueElement(element)) {
      break;
    }
  }

  // If no link was processed in the prior loop, but a link was found
  // using element.closest, then return that link. This can occur if the
  // link was a child of an <svg> element. This can also occur if the link
  // element is too deep in the ancestor tree.
  if (foundLinkElement) {
    sendFindElementAtPointResponse(
        requestId, getResponseForLinkElement(foundLinkElement));
    return true;
  }

  return false;
};

/**
 * Processes the element for a find element at point response.
 * @param {string} requestId An identifier which will be returned in the result
 *                 dictionary of this request.
 * @param {number} centerX The X coordinate of the center of the target.
 * @param {number} centerY The Y coordinate of the center of the target.
 * @param {Element!} element Element in the page.
 * @return {boolean} True if |element| was matched as the target of the touch.
 */
var processElementForFindElementAtPoint = function(
    requestId, centerX, centerY, element) {
  if (!element) {
    return false;
  }

  var tagName = element.tagName;
  if (!tagName) {
    return false;
  }

  tagName = tagName.toLowerCase();

  // if element is a frame, tell it to respond to this element request
  if (tagName === 'iframe' || tagName === 'frame') {
    var payload = {
      type: 'org.chromium.contextMenuMessage',
      requestId: requestId,
      x: centerX - element.offsetLeft,
      y: centerY - element.offsetTop
    };
    // The message will not be sent if |targetOrigin| is null, so use * which
    // allows the message to be delievered to the contentWindow regardless of
    // the origin.
    var targetOrigin = element.src || '*';
    element.contentWindow.postMessage(payload, targetOrigin);
    return true;
  }

  if (tagName === 'input' || tagName === 'textarea' || tagName === 'select' ||
      tagName === 'option') {
    // If the element is a known input element, stop the spiral search and
    // return empty results.
    sendFindElementAtPointResponse(requestId, /*response=*/ {});
    return true;
  }

  if (getComputedWebkitTouchCallout_(element) !== 'none') {
    if (tagName === 'a' && element.href) {
      sendFindElementAtPointResponse(
          requestId, getResponseForLinkElement(element));
      return true;
    }

    var imageSrc = getImageSource(element);
    if (imageSrc && !isTransparentElement(element)) {
      sendFindElementAtPointResponse(
          requestId, getResponseForImageElement(element, imageSrc));
      return true;
    }
  }

  return false;
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
    return href.animVal;
  }
  return href;
};


/**
 * Checks if the element is effectively transparent and should be skipped when
 * checking for image and link elements.
 * @param {Element!} element Element in the page.
 * @return {boolean} True if the element is transparent.
 */
var isTransparentElement = function(element) {
  var style = window.getComputedStyle(element);
  return isOpaque = style.getPropertyValue('display') === 'none' ||
      style.getPropertyValue('visibility') !== 'visible' ||
      Number(style.getPropertyValue('opacity')) <= TRANSPARENCY_THRESHOLD;
};

/**
 * Checks if the element blocks the user from viewing elements underneath it.
 * @param {Element!} element The element to check for opacity.
 * @return {boolean} True if the element blocks viewing of other elements.
 */
var isOpaqueElement = function(element) {
  var style = window.getComputedStyle(element);
  var isOpaque = style.getPropertyValue('display') !== 'none' &&
      style.getPropertyValue('visibility') === 'visible' &&
      Number(style.getPropertyValue('opacity')) >= OPACITY_THRESHOLD;

  // We consider an element opaque if it has a background color with an alpha
  // value of 1. To check this, we check to see if only rgb values are returned
  // or all rgba values are (the alpha value is only returned if it is not 1).
  var hasOpaqueBackgroundColor =
      style.getPropertyValue('background-color').split(', ').length === 3;
  var imageSource = getImageSource(element);
  var hasBackground = imageSource || hasOpaqueBackgroundColor;

  return isOpaque && hasBackground;
};

/**
 * Returns the image source if the element is an <img> or an element with a
 * background image.
 * @param {Element?} element The element from which to get the image source.
 * @return {string?} The image source, or null if no image source was found.
 */
var getImageSource = function(element) {
  if (element && element.tagName && element.tagName.toLowerCase() === 'img') {
    return element.currentSrc || element.src;
  }
  var backgroundImageString = window.getComputedStyle(element).backgroundImage;
  if (backgroundImageString === '' || backgroundImageString === 'none') {
    return null;
  }
  return extractUrlFromBackgroundImageString(backgroundImageString);
};

/**
 * Returns a url if it is the first background image in the string. Otherwise,
 * returns null.
 * @param {string} backgroundImageString String of the CSS background image
 *     property of an element.
 * @return {string?} The url of the first background image in
 * |backgroundImageString| or null if none are found.
 */
var extractUrlFromBackgroundImageString = function(backgroundImageString) {
  // backgroundImageString can contain more than one background image, some of
  // which may be urls or gradients.
  // Example: 'url("image1"), linear-gradient(#ffffff, #000000), url("image2")'
  var regex = /^url\(['|"]?(.+?)['|"]?\)/;
  var url = regex.exec(backgroundImageString);
  return url ? url[1] : null;
};

/**
 * Processes context menu messages received by the window.
 */
window.addEventListener('message', function(message) {
  var payload = message.data;
  if (payload.hasOwnProperty('type') &&
      payload.type == 'org.chromium.contextMenuMessage') {
    __gCrWeb.findElementAtPointInPageCoordinates(
        payload.requestId,
        payload.x + window.pageXOffset,
        payload.y + window.pageYOffset);
  }
});
