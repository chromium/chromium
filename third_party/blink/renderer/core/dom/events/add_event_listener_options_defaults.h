// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EVENTS_ADD_EVENT_LISTENER_OPTIONS_DEFAULTS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EVENTS_ADD_EVENT_LISTENER_OPTIONS_DEFAULTS_H_

namespace blink {

// Defines the default for 'passive' field used in the AddEventListenerOptions
// interface when javascript calls addEventListener.
// |False| is the default specified in
// https://dom.spec.whatwg.org/#dictdef-addeventlisteneroptions. However
// specifying a different default value is useful in demonstrating the
// power of passive event listeners.
enum class PassiveListenerDefault {
  kFalse,        // Default of false.
  kTrue,         // Default of true.
  kForceAllTrue  // Force all values to be true even when specified.
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EVENTS_ADD_EVENT_LISTENER_OPTIONS_DEFAULTS_H_
