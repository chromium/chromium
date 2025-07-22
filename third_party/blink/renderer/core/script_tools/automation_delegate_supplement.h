// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_AUTOMATION_DELEGATE_SUPPLEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_AUTOMATION_DELEGATE_SUPPLEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/script_tools/automation_delegate.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class LocalDOMWindow;

class CORE_EXPORT AutomationDelegateSupplement final
    : public GarbageCollected<AutomationDelegateSupplement>,
      public Supplement<LocalDOMWindow> {
 public:
  static const char kSupplementName[];

  static AutomationDelegateSupplement& From(LocalDOMWindow&);
  static AutomationDelegate* GetDelegateIfExists(LocalDOMWindow&);
  static AutomationDelegate* automationDelegate(LocalDOMWindow&);

  explicit AutomationDelegateSupplement(LocalDOMWindow&);
  AutomationDelegateSupplement(const AutomationDelegateSupplement&) = delete;
  AutomationDelegateSupplement& operator=(const AutomationDelegateSupplement&) =
      delete;

  void Trace(Visitor*) const override;

 private:
  AutomationDelegate* automationDelegate();

  Member<AutomationDelegate> automation_delegate_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_AUTOMATION_DELEGATE_SUPPLEMENT_H_
