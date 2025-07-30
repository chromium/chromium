// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script_tools/automation_delegate_supplement.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

// static
const char AutomationDelegateSupplement::kSupplementName[] =
    "AutomationDelegateSupplement";

// static
AutomationDelegateSupplement& AutomationDelegateSupplement::From(
    LocalDOMWindow& window) {
  AutomationDelegateSupplement* supplement =
      Supplement<LocalDOMWindow>::From<AutomationDelegateSupplement>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<AutomationDelegateSupplement>(window);
    ProvideTo(window, supplement);
  }
  return *supplement;
}

// static
AutomationDelegate* AutomationDelegateSupplement::GetDelegateIfExists(
    LocalDOMWindow& window) {
  AutomationDelegateSupplement* supplement =
      Supplement<LocalDOMWindow>::From<AutomationDelegateSupplement>(window);
  return supplement ? supplement->automationDelegate() : nullptr;
}

// static
AutomationDelegate* AutomationDelegateSupplement::automationDelegate(
    LocalDOMWindow& window) {
  return From(window).automationDelegate();
}

AutomationDelegateSupplement::AutomationDelegateSupplement(
    LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

void AutomationDelegateSupplement::Trace(Visitor* visitor) const {
  visitor->Trace(automation_delegate_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

AutomationDelegate* AutomationDelegateSupplement::automationDelegate() {
  if (!automation_delegate_) {
    automation_delegate_ = MakeGarbageCollected<AutomationDelegate>(
        GetSupplementable()->GetTaskRunner(TaskType::kUserInteraction));
  }
  return automation_delegate_.Get();
}

}  // namespace blink
