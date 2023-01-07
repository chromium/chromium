// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the attributes of the <webview> tag.

var $Element = require('safeMethods').SafeMethods.$Element;
var $MutationObserver = require('safeMethods').SafeMethods.$MutationObserver;
var GuestViewAttributes = require('guestViewAttributes').GuestViewAttributes;
var WebViewConstants = require('webViewConstants').WebViewConstants;
var WebViewInternal = getInternalApi('webViewInternal');

// -----------------------------------------------------------------------------
// AllowScalingAttribute object.

// Attribute that specifies whether scaling is allowed in the webview.
function AllowScalingAttribute(view) {
  $Function.call(
      GuestViewAttributes.BooleanAttribute, this,
      WebViewConstants.ATTRIBUTE_ALLOWSCALING, view);
}

AllowScalingAttribute.prototype.__proto__ =
    GuestViewAttributes.BooleanAttribute.prototype;

AllowScalingAttribute.prototype.handleMutation = function(oldValue, newValue) {
  if (!this.view.guest.getId())
  return;

  WebViewInternal.setAllowScaling(this.view.guest.getId(), this.getValue());
};

// -----------------------------------------------------------------------------
// AllowTransparencyAttribute object.

// Attribute that specifies whether transparency is allowed in the webview.
function AllowTransparencyAttribute(view) {
  $Function.call(
      GuestViewAttributes.BooleanAttribute, this,
      WebViewConstants.ATTRIBUTE_ALLOWTRANSPARENCY, view);
}

AllowTransparencyAttribute.prototype.__proto__ =
    GuestViewAttributes.BooleanAttribute.prototype;

AllowTransparencyAttribute.prototype.handleMutation = function(oldValue,
                                                               newValue) {
  if (!this.view.guest.getId())
    return;

  WebViewInternal.setAllowTransparency(this.view.guest.getId(),
                                       this.getValue());
};

// -----------------------------------------------------------------------------
// AutosizeDimensionAttribute object.

// Attribute used to define the demension limits of autosizing.
function AutosizeDimensionAttribute(name, view) {
  $Function.call(GuestViewAttributes.IntegerAttribute, this, name, view);
}

AutosizeDimensionAttribute.prototype.__proto__ =
    GuestViewAttributes.IntegerAttribute.prototype;

AutosizeDimensionAttribute.prototype.handleMutation = function(
    oldValue, newValue) {
  if (!this.view.guest.getId())
    return;

  this.view.guest.setSize({
    'enableAutoSize': this.view.attributes[
      WebViewConstants.ATTRIBUTE_AUTOSIZE].getValue(),
    'min': {
      'width': this.view.attributes[
          WebViewConstants.ATTRIBUTE_MINWIDTH].getValue(),
      'height': this.view.attributes[
          WebViewConstants.ATTRIBUTE_MINHEIGHT].getValue()
    },
    'max': {
      'width': this.view.attributes[
          WebViewConstants.ATTRIBUTE_MAXWIDTH].getValue(),
      'height': this.view.attributes[
          WebViewConstants.ATTRIBUTE_MAXHEIGHT].getValue()
    }
  });
  return;
};

// -----------------------------------------------------------------------------
// AutosizeAttribute object.

// Attribute that specifies whether the webview should be autosized.
function AutosizeAttribute(view) {
  $Function.call(
      GuestViewAttributes.BooleanAttribute, this,
      WebViewConstants.ATTRIBUTE_AUTOSIZE, view);
}

AutosizeAttribute.prototype.__proto__ =
    GuestViewAttributes.BooleanAttribute.prototype;

AutosizeAttribute.prototype.handleMutation =
    AutosizeDimensionAttribute.prototype.handleMutation;

// -----------------------------------------------------------------------------
// NameAttribute object.

// Attribute that sets the guest content's window.name object.
function NameAttribute(view) {
  $Function.call(
      GuestViewAttributes.Attribute, this, WebViewConstants.ATTRIBUTE_NAME,
      view);
}

NameAttribute.prototype.__proto__ = GuestViewAttributes.Attribute.prototype

NameAttribute.prototype.handleMutation = function(oldValue, newValue) {
  oldValue = oldValue || '';
  newValue = newValue || '';
  if (oldValue === newValue || !this.view.guest.getId())
    return;

  WebViewInternal.setName(this.view.guest.getId(), newValue);
};

NameAttribute.prototype.setValue = function(value) {
  value = value || '';
  if (value === '')
    $Element.removeAttribute(this.view.element, this.name);
  else
    $Element.setAttribute(this.view.element, this.name, value);
};

// -----------------------------------------------------------------------------
// PartitionAttribute object.

// Attribute representing the state of the storage partition.
function PartitionAttribute(view) {
  $Function.call(
      GuestViewAttributes.Attribute, this, WebViewConstants.ATTRIBUTE_PARTITION,
      view);
  this.validPartitionId = true;
}

PartitionAttribute.prototype.__proto__ =
    GuestViewAttributes.Attribute.prototype;

PartitionAttribute.prototype.handleMutation = function(oldValue, newValue) {
  newValue = newValue || '';

  // The partition cannot change if the webview has already navigated.
  if (!this.view.attributes[
          WebViewConstants.ATTRIBUTE_SRC].beforeFirstNavigation) {
    window.console.error(WebViewConstants.ERROR_MSG_ALREADY_NAVIGATED);
    this.setValueIgnoreMutation(oldValue);
    return;
  }
  if (newValue == 'persist:') {
    this.validPartitionId = false;
    window.console.error(
        WebViewConstants.ERROR_MSG_INVALID_PARTITION_ATTRIBUTE);
  }
};

PartitionAttribute.prototype.detach = function() {
  this.validPartitionId = true;
};

// -----------------------------------------------------------------------------
// SrcAttribute object.

// Attribute that handles the location and navigation of the webview.
function SrcAttribute(view) {
  $Function.call(
      GuestViewAttributes.Attribute, this, WebViewConstants.ATTRIBUTE_SRC,
      view);
  this.setupMutationObserver();
  this.beforeFirstNavigation = true;
}

SrcAttribute.prototype.__proto__ = GuestViewAttributes.Attribute.prototype;

SrcAttribute.prototype.setValueIgnoreMutation = function(value) {
  $Function.call(
      GuestViewAttributes.Attribute.prototype.setValueIgnoreMutation, this,
      value);
  // takeRecords() is needed to clear queued up src mutations. Without it, it is
  // possible for this change to get picked up asyncronously by src's mutation
  // observer |observer|, and then get handled even though we do not want to
  // handle this mutation.
  $MutationObserver.takeRecords(this.observer);
};

SrcAttribute.prototype.handleMutation = function(oldValue, newValue) {
  // Once we have navigated, we don't allow clearing the src attribute.
  // Once <webview> enters a navigated state, it cannot return to a
  // placeholder state.
  if (!newValue && oldValue) {
    // src attribute changes normally initiate a navigation. We suppress
    // the next src attribute handler call to avoid reloading the page
    // on every guest-initiated navigation.
    this.setValueIgnoreMutation(oldValue);
    return;
  }
  this.parse();
};

SrcAttribute.prototype.attach = function() {
  this.parse();
};

SrcAttribute.prototype.detach = function() {
  this.beforeFirstNavigation = true;
};

// The purpose of this mutation observer is to catch assignment to the src
// attribute without any changes to its value. This is useful in the case
// where the webview guest has crashed and navigating to the same address
// spawns off a new process.
SrcAttribute.prototype.setupMutationObserver = function() {
  this.observer = new $MutationObserver($Function.bind(function(mutations) {
    $Array.forEach(mutations, $Function.bind(function(mutation) {
      var oldValue = mutation.oldValue;
      var newValue = this.getValue();
      if (oldValue != newValue) {
        return;
      }
      this.handleMutation(oldValue, newValue);
    }, this));
  }, this));
  var params = {
    attributes: true,
    attributeOldValue: true,
    attributeFilter: [this.name]
  };
  $MutationObserver.observe(this.observer, this.view.element, params);
};

SrcAttribute.prototype.parse = function() {
  if (!this.view.elementAttached ||
      !this.view.attributes[
          WebViewConstants.ATTRIBUTE_PARTITION].validPartitionId ||
      !this.getValue()) {
    return;
  }

  if (!this.view.guest.getId()) {
    if (this.beforeFirstNavigation) {
      this.beforeFirstNavigation = false;
      this.view.createGuest();
    }
    return;
  }

  WebViewInternal.navigate(this.view.guest.getId(), this.getValue());
};

var WebViewAttributes = {
  AllowScalingAttribute: AllowScalingAttribute,
  AllowTransparencyAttribute: AllowTransparencyAttribute,
  AutosizeDimensionAttribute: AutosizeDimensionAttribute,
  AutosizeAttribute: AutosizeAttribute,
  NameAttribute: NameAttribute,
  PartitionAttribute: PartitionAttribute,
  SrcAttribute: SrcAttribute
};

// Exports.
exports.$set('WebViewAttributes', WebViewAttributes);
