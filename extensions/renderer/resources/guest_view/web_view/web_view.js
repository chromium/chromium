// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements WebView (<webview>) as a custom element that wraps a
// BrowserPlugin object element. The object element is hidden within
// the shadow DOM of the WebView element.

var $Element = require('safeMethods').SafeMethods.$Element;
var GuestView = require('guestView').GuestView;
var GuestViewContainer = require('guestViewContainer').GuestViewContainer;
var GuestViewInternalNatives = requireNative('guest_view_internal');
var WebViewConstants = require('webViewConstants').WebViewConstants;
var WebViewAttributes = require('webViewAttributes').WebViewAttributes;
var WebViewEvents = require('webViewEvents').WebViewEvents;
var WebViewInternal = getInternalApi('webViewInternal');

// Represents the internal state of <webview>.
function WebViewImpl(webviewElement) {
  $Function.call(GuestViewContainer, this, webviewElement, 'webview');
  this.pendingZoomFactor_ = null;
  this.userAgentOverride = null;
  this.setupElementProperties();
}

WebViewImpl.prototype.__proto__ = GuestViewContainer.prototype;

// Sets up all of the webview attributes.
WebViewImpl.prototype.setupAttributes = function() {
  this.attributes[WebViewConstants.ATTRIBUTE_ALLOWSCALING] =
      new WebViewAttributes.AllowScalingAttribute(this);
  this.attributes[WebViewConstants.ATTRIBUTE_ALLOWTRANSPARENCY] =
      new WebViewAttributes.AllowTransparencyAttribute(this);
  this.attributes[WebViewConstants.ATTRIBUTE_AUTOSIZE] =
      new WebViewAttributes.AutosizeAttribute(this);
  this.attributes[WebViewConstants.ATTRIBUTE_NAME] =
      new WebViewAttributes.NameAttribute(this);
  this.attributes[WebViewConstants.ATTRIBUTE_PARTITION] =
      new WebViewAttributes.PartitionAttribute(this);
  this.attributes[WebViewConstants.ATTRIBUTE_SRC] =
      new WebViewAttributes.SrcAttribute(this);

  var autosizeAttributes = [
    WebViewConstants.ATTRIBUTE_MAXHEIGHT, WebViewConstants.ATTRIBUTE_MAXWIDTH,
    WebViewConstants.ATTRIBUTE_MINHEIGHT, WebViewConstants.ATTRIBUTE_MINWIDTH
  ];
  for (var attribute of autosizeAttributes) {
    this.attributes[attribute] =
        new WebViewAttributes.AutosizeDimensionAttribute(attribute, this);
  }
};

WebViewImpl.prototype.setupEvents = function() {
  new WebViewEvents(this);
};

// Initiates navigation once the <webview> element is attached to the DOM.
WebViewImpl.prototype.onElementAttached = function() {
  // Mark all attributes as dirty on attachment.
  for (var i in this.attributes) {
    this.attributes[i].dirty = true;
  }
  for (var i in this.attributes) {
    this.attributes[i].attach();
  }
};

// Resets some state upon detaching <webview> element from the DOM.
WebViewImpl.prototype.onElementDetached = function() {
  for (var i in this.attributes) {
    this.attributes[i].dirty = false;
  }
  for (var i in this.attributes) {
    this.attributes[i].detach();
  }
};

// Sets the <webview>.request property.
WebViewImpl.prototype.setRequestPropertyOnWebViewElement = function(request) {
  $Object.defineProperty(
      this.element, 'request', {value: request, enumerable: true});
};

WebViewImpl.prototype.setupElementProperties = function() {
  // We cannot use {writable: true} property descriptor because we want a
  // dynamic getter value.
  $Object.defineProperty(this.element, 'contentWindow', {
    get: $Function.bind(
        function() {
          return this.guest.getContentWindow();
        },
        this),
    // No setter.
    enumerable: true
  });
};

WebViewImpl.prototype.onSizeChanged = function(webViewEvent) {
  var newWidth = webViewEvent.newWidth;
  var newHeight = webViewEvent.newHeight;

  var element = this.element;

  var width = element.offsetWidth;
  var height = element.offsetHeight;

  // Check the current bounds to make sure we do not resize <webview>
  // outside of current constraints.
  var maxWidth = this.attributes[
    WebViewConstants.ATTRIBUTE_MAXWIDTH].getValue() || width;
  var minWidth = this.attributes[
    WebViewConstants.ATTRIBUTE_MINWIDTH].getValue() || width;
  var maxHeight = this.attributes[
    WebViewConstants.ATTRIBUTE_MAXHEIGHT].getValue() || height;
  var minHeight = this.attributes[
    WebViewConstants.ATTRIBUTE_MINHEIGHT].getValue() || height;

  minWidth = Math.min(minWidth, maxWidth);
  minHeight = Math.min(minHeight, maxHeight);

  if (!this.attributes[WebViewConstants.ATTRIBUTE_AUTOSIZE].getValue() ||
      (newWidth >= minWidth &&
      newWidth <= maxWidth &&
      newHeight >= minHeight &&
      newHeight <= maxHeight)) {
    element.style.width = newWidth + 'px';
    element.style.height = newHeight + 'px';
    // Only fire the DOM event if the size of the <webview> has actually
    // changed.
    this.dispatchEvent(webViewEvent);
  }
};

WebViewImpl.prototype.createGuest = function() {
  this.guest.create(
      this.viewInstanceId, this.buildParams(), $Function.bind(function() {
        this.attachWindow();
      }, this));
};

WebViewImpl.prototype.onFrameNameChanged = function(name) {
  this.attributes[WebViewConstants.ATTRIBUTE_NAME].setValueIgnoreMutation(name);
};

// Updates state upon loadcommit.
WebViewImpl.prototype.onLoadCommit = function(
    baseUrlForDataUrl, currentEntryIndex, entryCount,
    processId, visibleUrl) {
  this.baseUrlForDataUrl = baseUrlForDataUrl;
  this.currentEntryIndex = currentEntryIndex;
  this.entryCount = entryCount;
  this.processId = processId;
  // Touching the src attribute triggers a navigation. To avoid
  // triggering a page reload on every guest-initiated navigation,
  // we do not handle this mutation.
  this.attributes[
      WebViewConstants.ATTRIBUTE_SRC].setValueIgnoreMutation(visibleUrl);
};

WebViewImpl.prototype.onAttach = function(storagePartitionId) {
  this.attributes[WebViewConstants.ATTRIBUTE_PARTITION].setValueIgnoreMutation(
      storagePartitionId);
};

WebViewImpl.prototype.buildContainerParams = function() {
  var params = $Object.create(null);
  params.initialZoomFactor = this.pendingZoomFactor_;
  params.userAgentOverride = this.userAgentOverride;
  for (var i in this.attributes) {
    var value = this.attributes[i].getValueIfDirty();
    if (value)
      params[i] = value;
  }
  return params;
};

WebViewImpl.prototype.attachWindow = function(opt_guestInstanceId) {
  // If |opt_guestInstanceId| was provided, then a different existing guest is
  // being attached to this webview, and the current one will get destroyed.
  if (opt_guestInstanceId) {
    if (this.guest.getId() == opt_guestInstanceId) {
      return;
    }
    this.guest.destroy();
    this.guest = new GuestView('webview', opt_guestInstanceId);
    this.prepareForReattach();
  }

  $Function.call(GuestViewContainer.prototype.attachWindow, this);
};

// Shared implementation of executeScript() and insertCSS().
WebViewImpl.prototype.executeCode = function(func, args) {
  if (!this.guest.getId()) {
    window.console.error(WebViewConstants.ERROR_MSG_CANNOT_INJECT_SCRIPT);
    return false;
  }

  // We specify what the embedder sees as the current URL, so that if this
  // inject call races with navigation in the guest, we don't inject into a
  // document we're not expecting.
  var webviewSrc = this.attributes[WebViewConstants.ATTRIBUTE_SRC].getValue();
  if (this.baseUrlForDataUrl) {
    // The virtual URL from the src attribute won't match the guest document's
    // URL when a base URL is provided for a data URL. The base URL should be
    // used for the comparison.
    webviewSrc = this.baseUrlForDataUrl;
  }

  args = $Array.concat([this.guest.getId(), webviewSrc],
                       $Array.slice(args));
  $Function.apply(func, null, args);
  return true;
};

WebViewImpl.prototype.setUserAgentOverride = function(userAgentOverride) {
  this.userAgentOverride = userAgentOverride;
  if (!this.guest.getId()) {
    // If we are not attached yet, then we will pick up the user agent on
    // attachment.
    return false;
  }
  WebViewInternal.overrideUserAgent(this.guest.getId(), userAgentOverride);
  return true;
};

WebViewImpl.prototype.loadDataWithBaseUrl = function(
    dataUrl, baseUrl, virtualUrl) {
  if (!this.guest.getId()) {
    return;
  }
  WebViewInternal.loadDataWithBaseUrl(
      this.guest.getId(), dataUrl, baseUrl, virtualUrl, function() {
        // Report any errors.
        if (chrome.runtime.lastError != undefined) {
          window.console.error(
              'Error while running webview.loadDataWithBaseUrl: ' +
              chrome.runtime.lastError.message);
        }
      });
};

WebViewImpl.prototype.setZoom = function(zoomFactor, callback) {
  if (!this.guest.getId()) {
    this.pendingZoomFactor_ = zoomFactor;
    return false;
  }
  this.pendingZoomFactor_ = null;
  WebViewInternal.setZoom(this.guest.getId(), zoomFactor, callback);
  return true;
};

// Requests the <webview> element wihtin the embedder to enter fullscreen.
WebViewImpl.prototype.makeElementFullscreen = function() {
  GuestViewInternalNatives.RunWithGesture($Function.bind(function() {
    $Element.webkitRequestFullScreen(this.element);
  }, this));
};

// Exports.
exports.$set('WebViewImpl', WebViewImpl);
