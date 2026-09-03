// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EVENTS_ADD_EVENT_LISTENER_OPTIONS_RESOLVED_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EVENTS_ADD_EVENT_LISTENER_OPTIONS_RESOLVED_H_

#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class AbortSignal;
class AddEventListenerOptions;

// AddEventListenerOptionsResolved class represents resolved event listener
// options. An application requests AddEventListenerOptions and the user
// agent may change ('resolve') these settings (based on settings or policies)
// and the result and the reasons why changes occurred are stored in this class.
class CORE_EXPORT AddEventListenerOptionsResolved {
  STACK_ALLOCATED();

 public:
  AddEventListenerOptionsResolved() = default;
  explicit AddEventListenerOptionsResolved(const AddEventListenerOptions*);

  bool Capture() const { return capture_; }
  void SetCapture(bool capture) { capture_ = capture; }

  bool HasPassive() const { return has_passive_; }
  bool Passive() const { return passive_; }
  void SetPassive(bool passive) {
    passive_ = passive;
    has_passive_ = true;
  }

  bool Once() const { return once_; }
  void SetOnce(bool once) { once_ = once; }

  bool HasSignal() const { return signal_ != nullptr; }
  AbortSignal* Signal() const { return signal_; }
  void SetSignal(AbortSignal* signal) { signal_ = signal; }

  void SetPassiveForcedForDocumentTarget(bool forced) {
    passive_forced_for_document_target_ = forced;
  }
  bool PassiveForcedForDocumentTarget() const {
    return passive_forced_for_document_target_;
  }

  // Set whether passive was specified when the options were
  // created by callee.
  void SetPassiveSpecified(bool specified) { passive_specified_ = specified; }
  bool PassiveSpecified() const { return passive_specified_; }

  void SetAnimationTrigger(bool val) { animation_trigger_ = val; }
  bool IsAnimationTrigger() const { return animation_trigger_; }

 private:
  AbortSignal* signal_ = nullptr;
  bool capture_ = false;
  bool passive_ = false;
  bool once_ = false;
  bool has_passive_ = false;
  bool passive_forced_for_document_target_{false};
  bool passive_specified_{false};
  bool animation_trigger_{false};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EVENTS_ADD_EVENT_LISTENER_OPTIONS_RESOLVED_H_
