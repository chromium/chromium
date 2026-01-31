// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_CHANGE_EVENT_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_CHANGE_EVENT_CONTROLLER_H_

#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/platform_event_controller.h"
#include "third_party/blink/renderer/core/page/focus_changed_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class MODULES_EXPORT ClipboardChangeEventController final
    : public GarbageCollected<ClipboardChangeEventController>,
      public Supplement<Navigator>,
      public PlatformEventController,
      public FocusChangedObserver {
 public:
  static const char kSupplementName[];
  explicit ClipboardChangeEventController(Navigator& navigator,
                                          EventTarget* eventTarget);

  // FocusChangedObserver overrides.
  void FocusedFrameChanged() override;

  ClipboardChangeEventController(const ClipboardChangeEventController&) =
      delete;
  ClipboardChangeEventController& operator=(
      const ClipboardChangeEventController&) = delete;

  ExecutionContext* GetExecutionContext() const;

  // PlatformEventController overrides.
  void DidUpdateData() override;
  void RegisterWithDispatcher() override;
  void UnregisterWithDispatcher() override;
  bool HasLastData() override;

  void Trace(Visitor*) const override;

 private:
  // Fires the clipboardchange event after page focus check.
  void OnClipboardChanged();
  void MaybeDispatchClipboardChangeEvent();
  void DispatchClipboardChangeEvent();

  // Callback for permission check result
  void OnPermissionResult(mojom::blink::PermissionStatus status);

  // Gets the SystemClipboard from the execution context.
  SystemClipboard* GetSystemClipboard() const;

  bool fire_clipboardchange_on_focus_ = false;
  Member<EventTarget> event_target_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_CHANGE_EVENT_CONTROLLER_H_
