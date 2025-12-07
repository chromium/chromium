// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the shared functionality for different guestview
// containers, such as web_view, app_view, etc.

var $parseInt = require('safeMethods').SafeMethods.$parseInt;
var $getComputedStyle = require('safeMethods').SafeMethods.$getComputedStyle;
var $Document = require('safeMethods').SafeMethods.$Document;
var $Element = require('safeMethods').SafeMethods.$Element;
var $EventTarget = require('safeMethods').SafeMethods.$EventTarget;
var $HTMLElement = require('safeMethods').SafeMethods.$HTMLElement;
var $Node = require('safeMethods').SafeMethods.$Node;
var GuestView = require('guestView').GuestView;
var GuestViewInternalNatives = requireNative('guest_view_internal');
var IdGenerator = requireNative('id_generator');
var MessagingNatives = requireNative('messaging_natives');

function GuestViewContainer(element, viewType) {
  this.attributes = $Object.create(null);
  this.element = element;
  this.elementAttached = false;
  this.viewInstanceId = IdGenerator.GetNextId();
  this.viewType = viewType;

  this.setupGuestProperty();
  this.guest = new GuestView(viewType);
  this.setupAttributes();
  this.setupEvents();

  this.internalElement = this.createInternalElement();
  this.shadowRoot = $Element.attachShadow(this.element, {mode: 'closed'});
  $Node.appendChild(this.shadowRoot, this.internalElement);

  GuestViewInternalNatives.RegisterView(this.viewInstanceId, this, viewType);
}

// Prevent GuestViewContainer inadvertently inheriting code from the global
// Object, allowing a pathway for executing unintended user code execution.
// TODO(wjmaclean): Track down other issues of Object inheritance.
// https://crbug.com/701034
GuestViewContainer.prototype.__proto__ = null;

// Create the 'guest' property to track new GuestViews and always listen for
// their resizes.
GuestViewContainer.prototype.setupGuestProperty = function() {
  $Object.defineProperty(this, 'guest', {
    get: $Function.bind(function() {
      return this.guest_;
    }, this),
    set: $Function.bind(function(value) {
      this.guest_ = value;
      if (!value) {
        return;
      }
      this.guest_.onresize = $Function.bind(function(e) {
        // Dispatch the 'contentresize' event.
        var contentResizeEvent = new Event('contentresize', { bubbles: true });
        contentResizeEvent.oldWidth = e.oldWidth;
        contentResizeEvent.oldHeight = e.oldHeight;
        contentResizeEvent.newWidth = e.newWidth;
        contentResizeEvent.newHeight = e.newHeight;
        this.dispatchEvent(contentResizeEvent);
      }, this);
    }, this),
    enumerable: true
  });
};

GuestViewContainer.prototype.createInternalElement = function() {
  var iframeElement = $Document.createElement(document, 'iframe');

  var style = $HTMLElement.style.get(iframeElement);
  $Object.defineProperty(style, 'width', {value: '100%'});
  $Object.defineProperty(style, 'height', {value: '100%'});
  $Object.defineProperty(style, 'border', {value: '0px'});

  return iframeElement;
};

GuestViewContainer.prototype.prepareForReattach = function() {
  // Since attachment swaps a local frame for a remote frame, we need our
  // internal iframe element to be local again before we can reattach.
  var newFrame = this.createInternalElement();
  var oldFrame = this.internalElement;
  this.internalElement = newFrame;
  var frameParent = $Node.parentNode.get(oldFrame);
  $Node.replaceChild(frameParent, newFrame, oldFrame);
};

GuestViewContainer.prototype.focus = function() {
  // Focus the internal element when focus() is called on the GuestView element.
  $HTMLElement.focus(this.internalElement);
}

GuestViewContainer.prototype.attachWindow = function() {
  var generatedId = IdGenerator.GetNextId();
  // Generate an instance id for the container.
  this.onInternalInstanceId(generatedId);
};

GuestViewContainer.prototype.makeGCOwnContainer = function(internalInstanceId) {
  MessagingNatives.BindToGC(this, function() {
    GuestViewInternalNatives.DestroyContainer(internalInstanceId);
  });
};

GuestViewContainer.prototype.onInternalInstanceId = function(
    internalInstanceId) {
  this.internalInstanceId = internalInstanceId;
  this.makeGCOwnContainer(this.internalInstanceId);

  if (!this.guest.getId()) {
    return;
  }
  this.guest.attach(this.internalInstanceId,
                    this.viewInstanceId,
                    this.buildParams());
};

GuestViewContainer.prototype.buildParams = function() {
  var params = this.buildContainerParams();
  params['instanceId'] = this.viewInstanceId;
  // When the GuestViewContainer is not participating in layout (display:none)
  // then getBoundingClientRect() would report a width and height of 0.
  // However, in the case where the GuestViewContainer has a fixed size we can
  // use that value to initially size the guest so as to avoid a relayout of the
  // on display:block.
  var css = $getComputedStyle(this.element, null);
  var elementRect = $Element.getBoundingClientRect(this.element);
  params['elementWidth'] =
      $parseInt(elementRect.width) || $parseInt(css.getPropertyValue('width'));
  params['elementHeight'] = $parseInt(elementRect.height) ||
      $parseInt(css.getPropertyValue('height'));
  return params;
};

GuestViewContainer.prototype.dispatchEvent = function(event) {
  return $EventTarget.dispatchEvent(this.element, event);
};

// Returns a wrapper function for |func| with a weak reference to |this|.
GuestViewContainer.prototype.weakWrapper = function(func) {
  var viewInstanceId = this.viewInstanceId;
  return function() {
    var view = GuestViewInternalNatives.GetViewFromID(viewInstanceId);
    if (view) {
      return $Function.apply(func, view, $Array.slice(arguments));
    }
  };
};

GuestViewContainer.prototype.willAttachElement = function() {
  if (this.deferredAttachCallback) {
    this.deferredAttachCallback();
    this.deferredAttachCallback = null;
  }
};

// Implemented by the specific view type, if needed.
GuestViewContainer.prototype.buildContainerParams = function() {
  return $Object.create(null);
};
GuestViewContainer.prototype.onElementAttached = function() {};
GuestViewContainer.prototype.onElementDetached = function() {};
GuestViewContainer.prototype.setupAttributes = function() {};
GuestViewContainer.prototype.setupEvents = function() {};

// Exports.
exports.$set('GuestViewContainer', GuestViewContainer);
