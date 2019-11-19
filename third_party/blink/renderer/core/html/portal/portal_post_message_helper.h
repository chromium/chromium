// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_POST_MESSAGE_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_POST_MESSAGE_HELPER_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class EventTarget;
class ExceptionState;
class ScriptState;
class ScriptValue;
class SecurityOrigin;
class WindowPostMessageOptions;
struct BlinkTransferableMessage;

class CORE_EXPORT PortalPostMessageHelper {
  STATIC_ONLY(PortalPostMessageHelper);

 public:
  static BlinkTransferableMessage CreateMessage(
      ScriptState* script_state,
      const ScriptValue& message,
      const WindowPostMessageOptions* options,
      ExceptionState& exception_state);

  static void CreateAndDispatchMessageEvent(
      EventTarget* target,
      BlinkTransferableMessage message,
      scoped_refptr<const SecurityOrigin> source_origin,
      scoped_refptr<const SecurityOrigin> target_origin);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_POST_MESSAGE_HELPER_H_
