// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var utils = require('utils');

function AutomationEventImpl(type, target, eventFrom, mouseX, mouseY, intents) {
  this.propagationStopped = false;
  this.type = type;
  this.target = target;
  this.eventPhase = Event.NONE;
  this.eventFrom = eventFrom;
  this.mouseX = mouseX;
  this.mouseY = mouseY;
  this.intents = intents;
}

AutomationEventImpl.prototype = {
  __proto__: null,
  stopPropagation: function() {
    this.propagationStopped = true;
  },
};

function AutomationEvent() {
  privates(AutomationEvent).constructPrivate(this, arguments);
}
utils.expose(AutomationEvent, AutomationEventImpl, {
  functions: [
    'stopPropagation',
  ],
  properties: [
    'generatedType',
  ],
  readonly: [
    'type',
    'target',
    'eventPhase',
    'eventFrom',
    'mouseX',
    'mouseY',
    'intents',
  ],
});

exports.$set('AutomationEvent', AutomationEvent);
