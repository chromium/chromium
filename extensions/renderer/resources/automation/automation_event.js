// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const utils = require('utils');

function AutomationEventImpl(
    type, target, eventFrom, eventFromAction, mouseX, mouseY, intents) {
  this.propagationStopped = false;
  this.type = type;
  this.target = target;
  this.eventPhase = Event.NONE;
  this.eventFrom = eventFrom;
  this.eventFromAction = eventFromAction;
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
    'eventFromAction',
    'mouseX',
    'mouseY',
    'intents',
  ],
});

exports.$set('AutomationEvent', AutomationEvent);
