// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var ExtensionOptionsConstants =
    require('extensionOptionsConstants').ExtensionOptionsConstants;
var ExtensionOptionsEvents =
    require('extensionOptionsEvents').ExtensionOptionsEvents;
var ExtensionOptionsAttributes =
    require('extensionOptionsAttributes').ExtensionOptionsAttributes;
var GuestViewContainer = require('guestViewContainer').GuestViewContainer;

function ExtensionOptionsImpl(extensionoptionsElement) {
  $Function.call(
      GuestViewContainer, this, extensionoptionsElement, 'extensionoptions');
};

ExtensionOptionsImpl.prototype.__proto__ = GuestViewContainer.prototype;

ExtensionOptionsImpl.prototype.onElementAttached = function() {
  this.createGuest();
};

// Sets up all of the extensionoptions attributes.
ExtensionOptionsImpl.prototype.setupAttributes = function() {
  this.attributes[ExtensionOptionsConstants.ATTRIBUTE_EXTENSION] =
      new ExtensionOptionsAttributes.ExtensionAttribute(this);
};

ExtensionOptionsImpl.prototype.setupEvents = function() {
  new ExtensionOptionsEvents(this);
};

ExtensionOptionsImpl.prototype.buildContainerParams = function() {
  var params = $Object.create(null);
  for (var i in this.attributes) {
    params[i] = this.attributes[i].getValue();
  }
  return params;
};

ExtensionOptionsImpl.prototype.createGuest = function() {
  // Destroy the old guest if one exists.
  this.guest.destroy($Function.bind(this.prepareForReattach, this));

  this.guest.create(
      this.viewInstanceId, this.buildParams(), $Function.bind(function() {
        if (!this.guest.getId()) {
          // Fire a createfailed event here rather than in ExtensionOptionsGuest
          // because the guest will not be created, and cannot fire an event.
          var createFailedEvent = new Event('createfailed', {bubbles: true});
          this.dispatchEvent(createFailedEvent);
        } else {
          this.attachWindow();
        }
      }, this));
};

// Exports.
exports.$set('ExtensionOptionsImpl', ExtensionOptionsImpl);
