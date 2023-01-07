// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var CreateEvent = require('guestViewEvents').CreateEvent;
var GuestViewEvents = require('guestViewEvents').GuestViewEvents;

function ExtensionOptionsEvents(extensionOptionsImpl) {
  $Function.call(GuestViewEvents, this, extensionOptionsImpl);

  // |setupEventProperty| is normally called automatically, but the
  // 'createfailed' event is registered here because the event is fired from
  // ExtensionOptionsImpl instead of in response to an extension event.
  this.setupEventProperty('createfailed');
}

ExtensionOptionsEvents.prototype.__proto__ = GuestViewEvents.prototype;

// A dictionary of <extensionoptions> extension events to be listened for. This
// dictionary augments |GuestViewEvents.EVENTS| in guest_view_events.js. See the
// documentation there for details.
ExtensionOptionsEvents.EVENTS = {
  'close': {
    evt: CreateEvent('extensionOptionsInternal.onClose')
  },
  'load': {
    evt: CreateEvent('extensionOptionsInternal.onLoad')
  },
  'preferredsizechanged': {
    evt: CreateEvent('extensionOptionsInternal.onPreferredSizeChanged'),
    fields:['width', 'height']
  }
}

ExtensionOptionsEvents.prototype.getEvents = function() {
  return ExtensionOptionsEvents.EVENTS;
};

// Exports.
exports.$set('ExtensionOptionsEvents', ExtensionOptionsEvents);
