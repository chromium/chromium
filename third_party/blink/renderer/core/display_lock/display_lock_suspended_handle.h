// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_SUSPENDED_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_SUSPENDED_HANDLE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class DisplayLockContext;
class CORE_EXPORT DisplayLockSuspendedHandle final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DisplayLockSuspendedHandle(DisplayLockContext* context);
  ~DisplayLockSuspendedHandle() override;

  // GC functions.
  void Trace(blink::Visitor*) override;
  void Dispose();

  // JavaScript interface implementation.
  void resume();

 private:
  WeakMember<DisplayLockContext> context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_SUSPENDED_HANDLE_H_
