/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_POINTER_LOCK_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_POINTER_LOCK_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class Element;
class Document;
class Page;
class PointerLockOptions;
class WebMouseEvent;

// This class handles mouse pointer lock and unlock, and dispatching mouse
// events when locked. See: https://w3c.github.io/pointerlock
class CORE_EXPORT PointerLockController final
    : public GarbageCollected<PointerLockController> {
 public:
  explicit PointerLockController(Page*);

  void RequestPointerLock(Element* target,
                          const PointerLockOptions* options = nullptr);
  void RequestPointerUnlock();
  void ElementRemoved(Element*);
  void DocumentDetached(Document*);
  bool LockPending() const;
  Element* GetElement() const;

  void DidAcquirePointerLock();
  void DidNotAcquirePointerLock();
  void DidLosePointerLock();
  void DispatchLockedMouseEvent(const WebMouseEvent&,
                                const Vector<WebMouseEvent>& coalesced_events,
                                const Vector<WebMouseEvent>& predicted_events,
                                const AtomicString& event_type);

  // Fetch the locked mouse position when pointer is locked. The values are not
  // changed if pointer is not locked.
  void GetPointerLockPosition(FloatPoint* lock_position,
                              FloatPoint* lock_screen_position);
  void Trace(blink::Visitor*);

 private:
  void ClearElement();
  void EnqueueEvent(const AtomicString& type, Element*);
  void EnqueueEvent(const AtomicString& type, Document*);

  Member<Page> page_;
  bool lock_pending_;
  Member<Element> element_;
  Member<Document> document_of_removed_element_while_waiting_for_unlock_;

  // Store the locked position so that the event position keeps unchanged when
  // in locked states. These values only get set when entering lock states.
  FloatPoint pointer_lock_position_;
  FloatPoint pointer_lock_screen_position_;

  DISALLOW_COPY_AND_ASSIGN(PointerLockController);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_POINTER_LOCK_CONTROLLER_H_
