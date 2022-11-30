// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the base attributes of the GuestView tags.

var $parseInt = require('safeMethods').SafeMethods.$parseInt;
var $Element = require('safeMethods').SafeMethods.$Element;

// -----------------------------------------------------------------------------
// Attribute object.

// Default implementation of a GuestView attribute.
function Attribute(name, view) {
  this.dirty = false;
  this.ignoreMutation = false;
  this.name = name;
  this.view = view;

  this.defineProperty();
}

// Prevent GuestViewEvents inadvertently inheritng code from the global Object,
// allowing a pathway for unintended execution of user code.
// TODO(wjmaclean): Track down other issues of Object inheritance.
// https://crbug.com/701034
Attribute.prototype.__proto__ = null;

// Retrieves and returns the attribute's value.
Attribute.prototype.getValue = function() {
  return $Element.getAttribute(this.view.element, this.name) || '';
};

// Retrieves and returns the attribute's value if it has been dirtied since
// the last time this method was called. Returns null otherwise.
Attribute.prototype.getValueIfDirty = function() {
  if (!this.dirty)
    return null;
  this.dirty = false;
  return this.getValue();
};

// Sets the attribute's value.
Attribute.prototype.setValue = function(value) {
  $Element.setAttribute(this.view.element, this.name, value || '');
};

// Changes the attribute's value without triggering its mutation handler.
Attribute.prototype.setValueIgnoreMutation = function(value) {
  this.ignoreMutation = true;
  this.setValue(value);
  this.ignoreMutation = false;
};

// Defines this attribute as a property on the view's element.
Attribute.prototype.defineProperty = function() {
  $Object.defineProperty(this.view.element, this.name, {
    get: $Function.bind(function() {
      return this.getValue();
    }, this),
    set: $Function.bind(function(value) {
      this.setValue(value);
    }, this),
    enumerable: true
  });
};

// Called when the attribute's value changes.
Attribute.prototype.maybeHandleMutation = function(oldValue, newValue) {
  if (this.ignoreMutation)
    return;

  this.dirty = true;
  this.handleMutation(oldValue, newValue);
};

// Called when a change that isn't ignored occurs to the attribute's value.
Attribute.prototype.handleMutation = function(oldValue, newValue) {};

// Called when the view's element is attached to the DOM tree.
Attribute.prototype.attach = function() {};

// Called when the view's element is detached from the DOM tree.
Attribute.prototype.detach = function() {};

// -----------------------------------------------------------------------------
// BooleanAttribute object.

// An attribute that is treated as a Boolean.
function BooleanAttribute(name, view) {
  $Function.call(Attribute, this, name, view);
}

BooleanAttribute.prototype.__proto__ = Attribute.prototype;

BooleanAttribute.prototype.getValue = function() {
  return $Element.hasAttribute(this.view.element, this.name);
};

BooleanAttribute.prototype.setValue = function(value) {
  if (!value) {
    $Element.removeAttribute(this.view.element, this.name);
  } else {
    $Element.setAttribute(this.view.element, this.name, '');
  }
};

// -----------------------------------------------------------------------------
// IntegerAttribute object.

// An attribute that is treated as an integer.
function IntegerAttribute(name, view) {
  $Function.call(Attribute, this, name, view);
}

IntegerAttribute.prototype.__proto__ = Attribute.prototype;

IntegerAttribute.prototype.getValue = function() {
  return $parseInt($Element.getAttribute(this.view.element, this.name)) || 0;
};

IntegerAttribute.prototype.setValue = function(value) {
  $Element.setAttribute(this.view.element, this.name, $parseInt(value) || 0);
};

// -----------------------------------------------------------------------------
// ReadOnlyAttribute object.

// An attribute that cannot be changed (externally). The only way to set it
// internally is via |setValueIgnoreMutation|.
function ReadOnlyAttribute(name, view) {
  $Function.call(Attribute, this, name, view);
}

ReadOnlyAttribute.prototype.__proto__ = Attribute.prototype;

ReadOnlyAttribute.prototype.handleMutation = function(oldValue, newValue) {
  this.setValueIgnoreMutation(oldValue);
}

// -----------------------------------------------------------------------------

var GuestViewAttributes = {
  Attribute: Attribute,
  BooleanAttribute: BooleanAttribute,
  IntegerAttribute: IntegerAttribute,
  ReadOnlyAttribute: ReadOnlyAttribute
};

// Exports.
exports.$set('GuestViewAttributes', GuestViewAttributes);
