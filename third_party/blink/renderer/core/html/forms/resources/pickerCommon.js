'use strict';
/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/**
 * @param {!string} id
 */
function $(id) {
  return document.getElementById(id);
}

/**
 * @param {!string} tagName
 * @param {string=} opt_class
 * @param {string=} opt_text
 * @return {!Element}
 */
function createElement(tagName, opt_class, opt_text) {
  var element = document.createElement(tagName);
  if (opt_class)
    element.setAttribute('class', opt_class);
  if (opt_text)
    element.appendChild(document.createTextNode(opt_text));
  return element;
}

/**
 * @constructor
 * @param {!number|Rectangle|Object} xOrRect
 * @param {!number} y
 * @param {!number} width
 * @param {!number} height
 */
function Rectangle(xOrRect, y, width, height) {
  if (typeof xOrRect === 'object') {
    y = xOrRect.y;
    width = xOrRect.width;
    height = xOrRect.height;
    xOrRect = xOrRect.x;
  }
  this.x = xOrRect;
  this.y = y;
  this.width = width;
  this.height = height;
}

Rectangle.prototype = {
  get maxX() {
    return this.x + this.width;
  },
  get maxY() {
    return this.y + this.height;
  },
  toString: function() {
    return 'Rectangle(' + this.x + ',' + this.y + ',' + this.width + ',' +
        this.height + ')';
  }
};

/**
 * @param {!Rectangle} rect1
 * @param {!Rectangle} rect2
 * @return {?Rectangle}
 */
Rectangle.intersection = function(rect1, rect2) {
  var x = Math.max(rect1.x, rect2.x);
  var maxX = Math.min(rect1.maxX, rect2.maxX);
  var y = Math.max(rect1.y, rect2.y);
  var maxY = Math.min(rect1.maxY, rect2.maxY);
  var width = maxX - x;
  var height = maxY - y;
  if (width < 0 || height < 0)
    return null;
  return new Rectangle(x, y, width, height);
};

/**
 * @param {!number} width in CSS pixel
 * @param {!number} height in CSS pixel
 */
function resizeWindow(width, height) {
  var zoom = global.params.zoomFactor ? global.params.zoomFactor : 1;
  setWindowRect(adjustWindowRect(
      width * zoom, height * zoom, width * zoom, height * zoom));
}

/**
 * @param {!number} width in DIP
 * @param {!number} height in DIP
 * @param {?number} minWidth in DIP
 * @param {?number} minHeight in DIP
 * @return {!Rectangle} Adjusted rectangle in DIP
 */
function adjustWindowRect(width, height, minWidth, minHeight) {
  if (typeof minWidth !== 'number')
    minWidth = 0;
  if (typeof minHeight !== 'number')
    minHeight = 0;

  var windowRect = new Rectangle(0, 0, Math.ceil(width), Math.ceil(height));

  if (!global.params.anchorRectInScreen)
    return windowRect;

  var anchorRect = new Rectangle(global.params.anchorRectInScreen);
  var availRect = new Rectangle(
      window.screen.availLeft, window.screen.availTop, window.screen.availWidth,
      window.screen.availHeight);

  _adjustWindowRectVertically(windowRect, availRect, anchorRect, minHeight);
  _adjustWindowRectHorizontally(windowRect, availRect, anchorRect, minWidth);

  return windowRect;
}

/**
 * Arguments are DIPs.
 */
function _adjustWindowRectVertically(
    windowRect, availRect, anchorRect, minHeight) {
  var availableSpaceAbove = anchorRect.y - availRect.y;
  availableSpaceAbove =
      Math.max(0, Math.min(availRect.height, availableSpaceAbove));

  var availableSpaceBelow = availRect.maxY - anchorRect.maxY;
  availableSpaceBelow =
      Math.max(0, Math.min(availRect.height, availableSpaceBelow));

  // In some situations, there may be no space available.  This can happen on
  // Linux when using a buggy window manager (https://crbug.com/774232).  When
  // this happens, don't try to constrain the window at all.
  if (availableSpaceAbove == 0 && availableSpaceBelow == 0) {
    windowRect.height = Math.max(minHeight, windowRect.height);
    windowRect.y = anchorRect.maxY;
  } else if (
      windowRect.height > availableSpaceBelow &&
      availableSpaceBelow < availableSpaceAbove) {
    windowRect.height = Math.min(windowRect.height, availableSpaceAbove);
    windowRect.height = Math.max(windowRect.height, minHeight);
    windowRect.y = anchorRect.y - windowRect.height;
  } else {
    windowRect.height = Math.min(windowRect.height, availableSpaceBelow);
    windowRect.height = Math.max(windowRect.height, minHeight);
    windowRect.y = anchorRect.maxY;
  }
}

/**
 * Arguments are DIPs.
 */
function _adjustWindowRectHorizontally(
    windowRect, availRect, anchorRect, minWidth) {
  if (anchorRect.maxX <= availRect.x || availRect.maxX <= anchorRect.x) {
    windowRect.width = Math.max(minWidth, windowRect.width);
    windowRect.x = anchorRect.x
    return;
  }
  windowRect.width = Math.min(windowRect.width, availRect.width);
  windowRect.width = Math.max(windowRect.width, minWidth);
  windowRect.x = anchorRect.x;
  // If we are getting clipped, we want to switch alignment to the right side
  // of the anchor rect as long as doing so will make the popup not clipped.
  var rightAlignedX = windowRect.x + anchorRect.width - windowRect.width;
  if (rightAlignedX >= availRect.x &&
      (windowRect.maxX > availRect.maxX || global.params.isRTL))
    windowRect.x = rightAlignedX;
}

/**
 * @param {!Rectangle} rect Window position and size in DIP.
 */
function setWindowRect(rect) {
  if (window.frameElement) {
    window.frameElement.style.width = rect.width + 'px';
    window.frameElement.style.height = rect.height + 'px';
  } else {
    window.pagePopupController.setWindowRect(
        rect.x, rect.y, rect.width, rect.height);
  }
}

function hideWindow() {
  setWindowRect(adjustWindowRect(1, 1, 1, 1));
}

/**
 * @return {!boolean}
 */
function isWindowHidden() {
  // window.innerWidth and innerHeight are zoom-adjusted values.  If we call
  // setWindowRect with width=100 and the zoom-level is 2.0, innerWidth will
  // return 50.
  return window.innerWidth <= 1 && window.innerHeight <= 1;
}

window.addEventListener('resize', function() {
  if (isWindowHidden())
    window.dispatchEvent(new CustomEvent('didHide'));
  else
    window.dispatchEvent(new CustomEvent('didOpenPicker'));
}, false);

/**
 * @return {!number}
 */
function getScrollbarWidth() {
  if (typeof window.scrollbarWidth === 'undefined') {
    var scrollDiv = document.createElement('div');
    scrollDiv.style.opacity = '0';
    scrollDiv.style.overflow = 'scroll';
    scrollDiv.style.width = '50px';
    scrollDiv.style.height = '50px';
    document.body.appendChild(scrollDiv);
    window.scrollbarWidth = scrollDiv.offsetWidth - scrollDiv.clientWidth;
    scrollDiv.parentNode.removeChild(scrollDiv);
  }
  return window.scrollbarWidth;
}

/**
 * @param {!string} className
 * @return {?Element}
 */
function enclosingNodeOrSelfWithClass(selfNode, className) {
  for (var node = selfNode; node && node !== selfNode.ownerDocument;
       node = node.parentNode) {
    if (node.nodeType === Node.ELEMENT_NODE &&
        node.classList.contains(className))
      return node;
  }
  return null;
}

/**
 * @constructor
 */
function EventEmitter() {
}

/**
 * @param {!string} type
 * @param {!function({...*})} callback
 */
EventEmitter.prototype.on = function(type, callback) {
  console.assert(callback instanceof Function);
  if (!this._callbacks)
    this._callbacks = {};
  if (!this._callbacks[type])
    this._callbacks[type] = [];
  this._callbacks[type].push(callback);
};

EventEmitter.prototype.hasListener = function(type) {
  if (!this._callbacks)
    return false;
  var callbacksForType = this._callbacks[type];
  if (!callbacksForType)
    return false;
  return callbacksForType.length > 0;
};

/**
 * @param {!string} type
 * @param {!function(Object)} callback
 */
EventEmitter.prototype.removeListener = function(type, callback) {
  if (!this._callbacks)
    return;
  var callbacksForType = this._callbacks[type];
  if (!callbacksForType)
    return;
  callbacksForType.splice(callbacksForType.indexOf(callback), 1);
  if (callbacksForType.length === 0)
    delete this._callbacks[type];
};

/**
 * @param {!string} type
 * @param {...*} var_args
 */
EventEmitter.prototype.dispatchEvent = function(type) {
  if (!this._callbacks)
    return;
  var callbacksForType = this._callbacks[type];
  if (!callbacksForType)
    return;
  callbacksForType = callbacksForType.slice(0);
  for (var i = 0; i < callbacksForType.length; ++i) {
    callbacksForType[i].apply(this, Array.prototype.slice.call(arguments, 1));
  }
};

/**
 * @constructor
 * @extends EventEmitter
 * @param {!Element} element
 * @param {!Object} config
 */
function Picker(element, config) {
  this._element = element;
  this._config = config;
}

Picker.prototype = Object.create(EventEmitter.prototype);

/**
 * @enum {number}
 */
Picker.Actions = {
  SetValue: 0,
  Cancel: -1,
  ChooseOtherColor: -2
};

/**
 * @param {!string} value
 */
Picker.prototype.submitValue = function(value) {
  window.pagePopupController.setValue(value);
  window.pagePopupController.closePopup();
};

Picker.prototype.handleCancel = function() {
  window.pagePopupController.closePopup();
};

Picker.prototype.chooseOtherColor = function() {
  window.pagePopupController.setValueAndClosePopup(
      Picker.Actions.ChooseOtherColor, '');
};

Picker.prototype.cleanup = function() {};

window.addEventListener('keyup', function(event) {
  // JAWS dispatches extra Alt events and unless we handle them they move the
  // focus and close the popup.
  if (event.key === 'Alt')
    event.preventDefault();
}, true);
