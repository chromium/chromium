// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview APIs used by CRWContextMenuController.
 */

import {getSurroundingText} from '//ios/web/js_features/context_menu/resources/surrounding_text.js';
import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js'

// The minimum opacity for an element to be considered as opaque. Elements
// with a higher opacity will prevent selection of images underneath.
const OPACITY_THRESHOLD = 0.9;

// The maximum opacity for an element to be considered as transparent.
// Images with a lower opacity will not be selectable.
const TRANSPARENCY_THRESHOLD = 0.1;

// The maximum depth to search for elements at any point.
const MAX_SEARCH_DEPTH = 20;

/**
 * Response from `findElementAtPoint` describing an image element.
 */
interface FindElementImgResult {
  // The request id passed to this JavaScript from the native application.
  // It is always included, but marked as optional because it is set after
  // creation.
  requestId?: string;
  // Lowercase tag of the element found ('img').
  tagName: string;
  // Referrer policy to use for navigations away from the current page.
  referrerPolicy: string;
  // URL source of the image.
  src: string;
  // URL of a link.
  href?: string;
  // Title of the image.
  title?: string;
  // The alternative text given with the image.
  alt?: string;
}

/**
 * Response from `findElementAtPoint` describing a link element.
 */
interface FindElementLinkResult {
  // The request id passed to this JavaScript from the native application.
  // It is always included, but marked as optional because it is set after
  // creation.
  requestId?: string;
  // Lowercase tag of the element found ('a')
  tagName: string;
  // Referrer policy to use for navigations away from the current page.
  referrerPolicy: string;
  // URL of a link if any.
  href: string;
  // The inner text of the link.
  innerText?: string;
}

/**
 * Response from `findElementAtPoint` describing any other element.
 */
interface FindElementTextResult {
  // The request id passed to this JavaScript from the native application.
  // It is always included, but marked as optional because it is set after
  // creation.
  requestId?: string;
  // Lowercase tag of the element found ('a')
  tagName: string;
  // The inner text of the element.
  innerText?: string;
  // This is defined for any text element and is an index to the tapped
  // character in `innerText`.
  textOffset?: number;
  // `surroundingText` and `surroundingTextOffset` before and after `innerText`.
  // Note that `innerText` is contained in `surroundingText`.
  surroundingText?: string;
  surroundingTextOffset?: number;
}

/**
 * Response from `findElementAtPoint` for failure to find any element.
 */
interface FindElementFailResult {
  requestId?: string;
}

type FindElementResult = FindElementImgResult|FindElementLinkResult|
    FindElementTextResult|FindElementFailResult;

/**
 * Represents local `x` and `y` coordinates in `window` space.
 */
class WindowCoordinates {
  public readonly viewPortX: number;
  public readonly viewPortY: number;

  constructor(public readonly x: number, public readonly y: number) {
    this.viewPortX = x - window.pageXOffset;
    this.viewPortY = y - window.pageYOffset;
  }
}

class Coordinates {
  constructor(public x: number, public y: number) {}
}

/**
 * Returns an object representing the details of a given link element.
 * @param element - the element whose details will be returned.
 */
function getResponseForLinkElement(element: HTMLAnchorElement|
                                   SVGAElement): FindElementLinkResult {
  return {
    tagName: 'a',
    href: getElementHref(element),
    referrerPolicy: getReferrerPolicy(element),
    innerText: element instanceof HTMLAnchorElement ? element.innerText :
                                                      undefined,
  };
}

/**
 * Returns an object representing the details of a given image element.
 * @param element - the element whose details will be returned.
 * @param src - the source of the image or image-like element.
 */
function getResponseForImageElement(
    element: HTMLElement, src: string): FindElementImgResult {
  const result: FindElementImgResult = {
    tagName: 'img',
    src: src,
    referrerPolicy: getReferrerPolicy(),
  };
  // Copy the title, if any.
  if (element.title) {
    result.title = element.title;
  }
  // Copy the alt text attribute, if any.
  if (element instanceof HTMLImageElement && element.alt) {
    result.alt = element.alt;
  }
  // Check if the image is also a link.
  let parent: Node|null = element.parentNode;
  while (parent) {
    if ((parent instanceof HTMLAnchorElement ||
         parent instanceof SVGAElement) &&
        getElementHref(parent)) {
      const href = getElementHref(parent);
      // This regex identifies strings like void(0),
      // void(0)  ;void(0);, ;;;;
      // which result in a NOP when executed as JavaScript.
      const regex = RegExp('^javascript:(?:(?:void\\(0\\)|;)\\s*)+$');
      if (href.match(regex)) {
        parent = parent.parentNode;
        continue;
      }
      result.href = href;
      result.referrerPolicy = getReferrerPolicy(parent);
      break;
    }
    parent = parent.parentNode;
  }
  return result;
}

/**
 * Returns an object representing the details of a given text element.
 * @param element - the element whose details will be returned.
 * @param x - horizontal center of the selected point in page coordinates.
 * @param y - Vertical center of the selected point in page coordinates.
 */
function getResponseForTextElement(
    element: Element, x: number, y: number): FindElementTextResult {
  const result: FindElementTextResult = {
    tagName: element.tagName,
  };

  // caretRangeFromPoint is custom WebKit method.
  if (document.caretRangeFromPoint) {
    const range = document.caretRangeFromPoint(x, y);
    if (range && range.startContainer) {
      const textNode = range.startContainer;
      if (textNode.nodeType === Node.TEXT_NODE) {
        result.textOffset = range.startOffset;
        result.innerText = textNode.nodeValue ?? '';
        const textAndStartPos = getSurroundingText(range);
        result.surroundingText = textAndStartPos.text;
        result.surroundingTextOffset = textAndStartPos.position;
      }
    }
  }
  return result;
}

/**
 * Finds the url of the image or link under the selected point. Sends a
 * `FindElementResult` with details the found element (or an empty object
 * if nothing is found) back to the application by posting a
 * 'FindElementResult' message.
 * @param requestId - an identifier which will be returned in the result
 *                 dictionary of this request.
 * @param x - horizontal center of the selected point in page
 *                 coordinates.
 * @param y - vertical center of the selected point in page
 *                 coordinates.
 */
function findElementAtPointInPageCoordinates(
    requestId: string, x: number, y: number) {
  const hitCoordinates = spiralCoordinates(x, y);
  const processedElements = new Set<Element>();
  for (let coordinates of hitCoordinates) {
    const coordinateDetails =
        new WindowCoordinates(coordinates.x, coordinates.y);
    const useViewPortCoordinates = elementFromPointIsUsingViewPortCoordinates();
    const coordinateX = useViewPortCoordinates ? coordinateDetails.viewPortX :
                                                 coordinateDetails.x;
    const coordinateY = useViewPortCoordinates ? coordinateDetails.viewPortY :
                                                 coordinateDetails.y;
    const elementWasFound = findElementAtPoint(
        requestId, window.document, processedElements, coordinateX, coordinateY,
        x, y);

    // Exit early if an element was found.
    if (elementWasFound) {
      return;
    }
  }

  // If no element was found, send an empty response.
  sendFindElementAtPointResponse(requestId, /*response=*/ {});
}

/**
 * Finds the topmost valid element at the given point. returns whether or not an
 * element was matched as a target of the touch.
 * @param requestId - an identifier which will be returned in the result
 *                 dictionary of this request.
 * @param root - the Document or ShadowRoot object to search within.
 * @param processedElements - a set to store processed elements in.
 * @param pointX - the X coordinate of the target location.
 * @param pointY - the Y coordinate of the target location.
 * @param centerX - the X coordinate of the center of the target.
 * @param centerY - the Y coordinate of the center of the target.
 */
function findElementAtPoint(
    requestId: string, root: Document|ShadowRoot,
    processedElements: Set<Element>, pointX: number, pointY: number,
    centerX: number, centerY: number): boolean {
  // Make chrome_annotation temporary available for `elementsFromPoint`.
  const annotations = document.querySelectorAll('chrome_annotation');
  for (let annotation of annotations) {
    if (annotation instanceof HTMLElement)
      annotation.style.pointerEvents = 'all';
  }
  const elements = root.elementsFromPoint(pointX, pointY);
  for (let annotation of annotations) {
    if (annotation instanceof HTMLElement)
      annotation.style.pointerEvents = 'none';
  }
  let foundLinkElement: HTMLAnchorElement|SVGAElement|null = null;
  let foundTextElement: Element|null = null;
  let foundImageElement: HTMLElement|null = null;
  for (let elementIndex = 0;
       elementIndex < elements.length && elementIndex < MAX_SEARCH_DEPTH;
       elementIndex++) {
    const element: Element = elements[elementIndex]!;

    // Element.closest will find link elements that are parents of the current
    // element. It also works for SVGAElements, links within svg tags. However,
    // we must still iterate through the elements at this position to find
    // images. This ensures that it will never miss the link, even if this loop
    //  terminates due to hitting an opaque element.
    if (!foundLinkElement) {
      const closestLink = element.closest('a');
      if (closestLink && closestLink.href &&
          getComputedWebkitTouchCallout(closestLink) !== 'none') {
        foundLinkElement = closestLink;
      }
    }

    if (!processedElements.has(element)) {
      processedElements.add(element);
      if (element.shadowRoot) {
        // The element's shadowRoot can be the same as `root` if the point is
        // not on any child element. Break the recursion and return no found
        // element.
        if (element.shadowRoot === root) {
          return false;
        }

        // If an element is found in the shadow DOM, return true, otherwise
        // keep iterating.
        if (findElementAtPoint(
                requestId, element.shadowRoot, processedElements, pointX,
                pointY, centerX, centerY)) {
          return true;
        }
      }

      if (processElementForFindElementAtPoint(
              requestId, centerX, centerY, element as HTMLElement)) {
        return true;
      }
    }

    if (getComputedWebkitTouchCallout(element) !== 'none') {
      // Remember topmost text element, while going up the tree looking for
      // links.
      if (foundTextElement === null && element.tagName !== 'HTML' &&
          element.tagName !== 'IMG' && element.tagName !== 'svg' &&
          isTextElement(element)) {
        foundTextElement = element;
      }

      // Remember topmost opaque image, while going up the tree looking for
      // links. If there's already a topmost text, no need to remember this
      // image.
      if (foundImageElement === null && foundTextElement === null &&
          getImageSource(element) && !isTransparentElement(element)) {
        foundImageElement = element as HTMLElement;
      }
    }

    // Opaque elements should block taps on images that are behind them.
    if (isOpaqueElement(element)) {
      break;
    }
  }

  if (foundImageElement) {
    // imageSrc cannot be null, as it would've stopped `foundImageElement` from
    // being set.
    const imageSrc = getImageSource(foundImageElement);
    sendFindElementAtPointResponse(
        requestId, getResponseForImageElement(foundImageElement, imageSrc!));
    return true;
  }

  if (foundLinkElement) {
    // If no link was processed in the prior loop, but a link was found
    // using element.closest, then return that link. This can occur if the
    // link was a child of an <svg> element. This can also occur if the link
    // element is too deep in the ancestor tree.
    sendFindElementAtPointResponse(
        requestId, getResponseForLinkElement(foundLinkElement));
    return true;
  }

  if (foundTextElement) {
    sendFindElementAtPointResponse(
        requestId,
        getResponseForTextElement(
            foundTextElement, centerX - window.pageXOffset,
            centerY - window.pageYOffset));
    return true;
  }

  return false;
}

/**
 * Processes the element for a find element at point response and return true
 * if `element` was matched as the target of the touch. Only links and input
 * elements will stop the process right away. Frames will make it continue
 * inside the frame.
 * @param requestId - an identifier which will be returned in the result
 *                 dictionary of this request.
 * @param centerX - the X coordinate of the center of the target.
 * @param centerY - the Y coordinate of the center of the target.
 * @param element - element in the page.
 */
function processElementForFindElementAtPoint(
    requestId: string, centerX: number, centerY: number,
    element: HTMLElement|null): boolean {
  if (!element || !element.tagName) {
    return false;
  }

  const tagName = element.tagName.toLowerCase();

  // if element is a frame, tell it to respond to this element request
  if (tagName === 'iframe' || tagName === 'frame') {
    const payload = {
      type: 'org.chromium.contextMenuMessage',
      requestId: requestId,
      x: centerX - element.offsetLeft,
      y: centerY - element.offsetTop,
    };
    // The message will not be sent if `targetOrigin` is null or "about:blank",
    // so use * which allows the message to be delievered to the contentWindow
    // regardless of the origin.
    if (element instanceof HTMLIFrameElement) {
      let targetOrigin = '*';
      const iframeSrc = element.src;
      if (iframeSrc && !iframeSrc.startsWith('about:')) {
        targetOrigin = iframeSrc;
      }
      if (element.contentWindow) {
        element.contentWindow.postMessage(payload, targetOrigin);
      }
    }
    return true;
  }

  if (tagName === 'input' || tagName === 'textarea' || tagName === 'select' ||
      tagName === 'option') {
    // If the element is a known input element, stop the spiral search and
    // return empty results.
    sendFindElementAtPointResponse(requestId, /*response=*/ {});
    return true;
  }

  if (getComputedWebkitTouchCallout(element) !== 'none') {
    if (tagName === 'a' &&
        (element instanceof HTMLAnchorElement ||
         element instanceof SVGAElement) &&
        element.href) {
      sendFindElementAtPointResponse(
          requestId, getResponseForLinkElement(element));
      return true;
    }
  }

  return false;
}

/**
 * Returns true if given node has at least one non empty child text node.
 */
function isTextElement(node: Node) {
  if (!node.hasChildNodes())
    return false;
  for (let subnode of node.childNodes) {
    if (subnode.nodeType === Node.TEXT_NODE && subnode.textContent !== '')
      return true;
  }
  return false;
}

/**
 * Inserts |requestId| into |response| and sends the result as the payload of a
 * 'FindElementResult' message back to the native application.
 * @param requestId - an identifier which will be returned in the result
 *                 dictionary of this request.
 * @param response - the 'FindElementResult' message payload.
 */
function sendFindElementAtPointResponse(
    requestId: string, response: FindElementResult): void {
  response.requestId = requestId;
  sendWebKitMessage('FindElementResultHandler', response);
}

/**
 * Returns whether or not view port coordinates should be used for the given
 * window (if the window has been scrolled down or to the right).
 */
function elementFromPointIsUsingViewPortCoordinates(): boolean {
  if (window.pageYOffset > 0) {  // Page scrolled down.
    return (
        window.document.elementFromPoint(
            0, window.pageYOffset + window.innerHeight - 1) === null);
  }
  if (window.pageXOffset > 0) {  // Page scrolled to the right.
    return (
        window.document.elementFromPoint(
            window.pageXOffset + window.innerWidth - 1, 0) === null);
  }
  return false;  // No scrolling, don't care.
}

/**
 * Returns spiral coordinates around `x` and `y`.
 */
function spiralCoordinates(x: number, y: number): Coordinates[] {
  const maxAngle = Math.PI * 2.0 * 2.0;
  const pointCount = 10;
  const angleStep = maxAngle / pointCount;
  const touchMargin = 15;
  const speed = touchMargin / maxAngle;

  const coordinates: Coordinates[] = [];
  for (let index = 0; index < pointCount; index++) {
    const angle = angleStep * index;
    const radius = angle * speed;

    coordinates.push(new Coordinates(
        x + Math.round(Math.cos(angle) * radius),
        y + Math.round(Math.sin(angle) * radius)));
  }

  return coordinates;
}

/**
 * Returns webkitTouchCallout styles or 'none'.
 */
function getComputedWebkitTouchCallout(element: Element): string {
  // webkitTouchCallout isn't an 'official' property, so it must be retrieved
  // by name. Casting `CSSStyleDecoration` as dictionary to avoid error TS7015
  // (Element implicitly has an 'any' type because index expression is not of
  // type 'number').
  const styles = window.getComputedStyle(element, null) as {[key: string]: any};
  return styles['webkitTouchCallout'];
}

/**
 * Gets the referrer policy to use for navigations away from the current page.
 * If a link element is passed, and it includes a rel=noreferrer tag, that
 * will override the page setting.
 * @param linkElement - optional link triggering the navigation.
 */
function getReferrerPolicy(linkElement?: Element): string {
  if (linkElement) {
    const rel = linkElement.getAttribute('rel');
    if (rel && rel.toLowerCase() === 'noreferrer') {
      return 'never';
    }
  }

  // Search for referrer meta tag.  WKWebView only supports a subset of values
  // for referrer meta tags.  If it parses a referrer meta tag with an
  // unsupported value, it will default to 'never'.
  const metaTags = document.getElementsByTagName('meta');
  for (const metaTag of metaTags) {
    if (metaTag.name.toLowerCase() === 'referrer') {
      const referrerPolicy = metaTag.content.toLowerCase();
      if (referrerPolicy === 'default' || referrerPolicy === 'always' ||
          referrerPolicy === 'no-referrer' || referrerPolicy === 'origin' ||
          referrerPolicy === 'no-referrer-when-downgrade' ||
          referrerPolicy === 'unsafe-url') {
        return referrerPolicy;
      } else {
        return 'never';
      }
    }
  }
  return 'default';
}

/**
 * Returns the href of the given element. Handles standard <a> links as well as
 * xlink:href links as used within <svg> tags.
 * @param element - the link triggering the navigation.
 */
function getElementHref(element: HTMLAnchorElement|SVGAElement): string {
  const href = element.href;
  if (href instanceof SVGAnimatedString) {
    return href.animVal;
  }
  return href;
}


/**
 * Checks if the element is effectively transparent and should be skipped when
 * checking for image and link elements.
 * @param element - element in the page.
 */
function isTransparentElement(element: Element): boolean {
  const style = window.getComputedStyle(element);
  return style.getPropertyValue('display') === 'none' ||
      style.getPropertyValue('visibility') !== 'visible' ||
      Number(style.getPropertyValue('opacity')) <= TRANSPARENCY_THRESHOLD;
}

/**
 * Checks if the element blocks the user from viewing elements underneath it.
 * @param element - the element to check for opacity.
 */
function isOpaqueElement(element: Element): boolean {
  const style = window.getComputedStyle(element);
  const isOpaque = style.getPropertyValue('display') !== 'none' &&
      style.getPropertyValue('visibility') === 'visible' &&
      Number(style.getPropertyValue('opacity')) >= OPACITY_THRESHOLD;

  // We consider an element opaque if it has a background color with an alpha
  // value of 1. To check this, we check to see if only rgb values are returned
  // or all rgba values are (the alpha value is only returned if it is not 1).
  const hasOpaqueBackgroundColor =
      style.getPropertyValue('background-color').split(', ').length === 3;
  const imageSource = getImageSource(element);
  const hasBackground = !!imageSource || hasOpaqueBackgroundColor;

  return isOpaque && hasBackground;
}

/**
 * Returns the image source if the element is an <img> or an element with a
 * background image. Returns null if no image source was found.
 * @param element - the element from which to get the image source.
 */
function getImageSource(element: Element): string|null {
  if (element.tagName && element.tagName.toLowerCase() === 'img') {
    if (element instanceof HTMLImageElement) {
      return element.currentSrc || element.src;
    }
  }
  const backgroundImageString =
      window.getComputedStyle(element).backgroundImage;
  if (backgroundImageString === '' || backgroundImageString === 'none') {
    return null;
  }
  return extractUrlFromBackgroundImageString(backgroundImageString);
}

/**
 * Returns a url if it is the first background image in the string. Otherwise,
 * returns null.
 * @param backgroundImageString - string of the CSS background image
 *     property of an element.
 */
function extractUrlFromBackgroundImageString(backgroundImageString: string):
    string|null {
  // backgroundImageString can contain more than one background image, some of
  // which may be urls or gradients.
  // Example: 'url("image1"), linear-gradient(#ffffff, #000000), url("image2")'
  const regex = /^url\(['|"]?(.+?)['|"]?\)/;
  const url = regex.exec(backgroundImageString);
  return url && url[1] ? url[1] : null;
}

/**
 * Processes context menu messages received by the window.
 */
window.addEventListener('message', function(message) {
  const payload = message.data;
  if (!payload || typeof payload !== 'object') {
    return;
  }
  if (payload.hasOwnProperty('type') &&
      payload.type === 'org.chromium.contextMenuMessage') {
    findElementAtPointInPageCoordinates(
        payload.requestId, payload.x + window.pageXOffset,
        payload.y + window.pageYOffset);
  }
});

// Call contextMenuAllFrames on gCrWeb directly to prevent code duplication
// that using export/import would create.
gCrWeb.contextMenuAllFrames = {
  findElementAtPointInPageCoordinates,
  // For testing only:
  getSurroundingText,
};
