// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class AutomationEvent {
  constructor(
      type, target, eventFrom, eventFromAction, mouseX, mouseY, intents) {
    this.propagationStopped_ = false;
    this.type_ = type;
    this.target_ = target;
    this.eventPhase_ = Event.NONE;
    this.eventFrom_ = eventFrom;
    this.eventFromAction_ = eventFromAction;
    this.mouseX_ = mouseX;
    this.mouseY_ = mouseY;
    this.intents_ = intents;
  }

  stopPropagation() {
    this.propagationStopped_ = true;
  }

  get propagationStopped() {
    return this.propagationStopped_;
  }
  get type() {
    return this.type_;
  }
  get target() {
    return this.target_;
  }
  get eventPhase() {
    return this.eventPhase_;
  }
  set eventPhase(phase) {
    this.eventPhase_ = phase;
  }
  get eventFrom() {
    return this.eventFrom_;
  }
  get eventFromAction() {
    return this.eventFromAction_;
  }
  get mouseX() {
    return this.mouseX_;
  }
  get mouseY() {
    return this.mouseY_;
  }
  get intents() {
    return this.intents_;
  }
}

exports.$set('AutomationEvent', AutomationEvent);
