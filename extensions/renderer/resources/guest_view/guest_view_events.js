// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Event management for GuestViewContainers.

var $EventTarget = require('safeMethods').SafeMethods.$EventTarget;
var GuestViewInternalNatives = requireNative('guest_view_internal');
var MessagingNatives = requireNative('messaging_natives');

var CreateEvent = function(name) {
  return bindingUtil.createCustomEvent(name,
                                       true /* supportsFilters */,
                                       false /* supportsLazyListeners */);
};

function GuestViewEvents(view) {
  view.events = this;

  this.view = view;
  this.on = $Object.create(null);

  // |setupEventProperty| is normally called automatically, but these events are
  // are registered here because they are dispatched from GuestViewContainer
  // instead of in response to extension events.
  this.setupEventProperty('contentresize');
  this.setupEventProperty('resize');
  this.setupEvents();
}

// Prevent GuestViewEvents inadvertently inheritng code from the global Object,
// allowing a pathway for unintended execution of user code.
// TODO(wjmaclean): Track down other issues of Object inheritance.
// https://crbug.com/701034
GuestViewEvents.prototype.__proto__ = null;

// |GuestViewEvents.EVENTS| is a dictionary of extension events to be listened
//     for, which specifies how each event should be handled. The events are
//     organized by name, and by default will be dispatched as DOM events with
//     the same name.
// |cancelable| (default: false) specifies whether the DOM event's default
//     behavior can be canceled. If the default action associated with the event
//     is prevented, then its dispatch function will return false in its event
//     handler. The event must have a specified |handler| for this to be
//     meaningful.
// |evt| specifies a descriptor object for the extension event. An event
//     listener will be attached to this descriptor.
// |fields| (default: none) specifies the public-facing fields in the DOM event
//     that are accessible to developers.
// |handler| specifies the name of a handler function to be called each time
//     that extension event is caught by its event listener. The DOM event
//     should be dispatched within this handler function (if desired). With no
//     handler function, the DOM event will be dispatched by default each time
//     the extension event is caught.
// |internal| (default: false) specifies that the event will not be dispatched
//     as a DOM event, and will also not appear as an on* property on the view’s
//     element. A |handler| should be specified for all internal events, and
//     |fields| and |cancelable| should be left unspecified (as they are only
//     meaningful for DOM events).
GuestViewEvents.EVENTS = $Object.create(null);

// Attaches |listener| onto the event descriptor object |evt|, and registers it
// to be removed once this GuestViewEvents object is garbage collected.
GuestViewEvents.prototype.addScopedListener = function(
    evt, listener, listenerOpts) {
  $Array.push(this.listenersToBeRemoved, { 'evt': evt, 'listener': listener });
  evt.addListener(listener, listenerOpts);
};

// Sets up the handling of events.
GuestViewEvents.prototype.setupEvents = function() {
  // An array of registerd event listeners that should be removed when this
  // GuestViewEvents is garbage collected.
  this.listenersToBeRemoved = [];
  MessagingNatives.BindToGC(
      this, $Function.bind(function(listenersToBeRemoved) {
    for (var i = 0; i != listenersToBeRemoved.length; ++i) {
      listenersToBeRemoved[i].evt.removeListener(
          listenersToBeRemoved[i].listener);
      listenersToBeRemoved[i] = null;
    }
  }, undefined, this.listenersToBeRemoved), -1 /* portId */);

  // Set up the GuestView events.
  for (var eventName in GuestViewEvents.EVENTS) {
    this.setupEvent(eventName, GuestViewEvents.EVENTS[eventName]);
  }

  // Set up the derived view's events.
  var events = this.getEvents();
  for (var eventName in events) {
    this.setupEvent(eventName, events[eventName]);
  }
};

// Sets up the handling of the |eventName| event.
GuestViewEvents.prototype.setupEvent = function(eventName, eventInfo) {
  if (!eventInfo.internal) {
    this.setupEventProperty(eventName);
  }

  var listenerOpts = { instanceId: this.view.viewInstanceId };
  if (eventInfo.handler) {
    this.addScopedListener(eventInfo.evt, this.weakWrapper(function(e) {
      this[eventInfo.handler](e, eventName);
    }), listenerOpts);
    return;
  }

  // Internal events are not dispatched as DOM events.
  if (eventInfo.internal) {
    return;
  }

  this.addScopedListener(eventInfo.evt, this.weakWrapper(function(e) {
    var domEvent = this.makeDomEvent(e, eventName);
    this.view.dispatchEvent(domEvent);
  }), listenerOpts);
};

// Constructs a DOM event based on the info for the |eventName| event provided
// in either |GuestViewEvents.EVENTS| or getEvents().
GuestViewEvents.prototype.makeDomEvent = function(event, eventName) {
  var eventInfo =
      GuestViewEvents.EVENTS[eventName] || this.getEvents()[eventName];

  // Internal events are not dispatched as DOM events.
  if (eventInfo.internal) {
    return null;
  }

  var details = $Object.create(null);
  details.bubbles = true;
  if (eventInfo.cancelable) {
    details.cancelable = true;
  }
  var domEvent = new Event(eventName, details);
  if (eventInfo.fields) {
    $Array.forEach(eventInfo.fields, $Function.bind(function(field) {
      if (event[field] !== undefined) {
        $Object.defineProperty(domEvent, field, {value: event[field]});
      }
    }, this));
  }

  return domEvent;
};

// Adds an 'on<event>' property on the view, which can be used to set/unset
// an event handler.
GuestViewEvents.prototype.setupEventProperty = function(eventName) {
  var propertyName = 'on' + $String.toLowerCase(eventName);
  $Object.defineProperty(this.view.element, propertyName, {
    get: $Function.bind(function() {
      return this.on[propertyName];
    }, this),
    set: $Function.bind(function(value) {
      if (this.on[propertyName]) {
        $EventTarget.removeEventListener(
            this.view.element, eventName, this.on[propertyName]);
      }
      this.on[propertyName] = value;
      if (value) {
        $EventTarget.addEventListener(this.view.element, eventName, value);
      }
    }, this),
    enumerable: true
  });
};

// returns a wrapper for |func| with a weak reference to |this|.
GuestViewEvents.prototype.weakWrapper = function(func) {
  var viewInstanceId = this.view.viewInstanceId;
  return function() {
    var view = GuestViewInternalNatives.GetViewFromID(viewInstanceId);
    if (!view) {
      return;
    }
    return $Function.apply(func, view.events, $Array.slice(arguments));
  };
};

// Implemented by the derived event manager, if one exists.
GuestViewEvents.prototype.getEvents = function() { return {}; };

// Exports.
exports.$set('GuestViewEvents', GuestViewEvents);
exports.$set('CreateEvent', CreateEvent);
