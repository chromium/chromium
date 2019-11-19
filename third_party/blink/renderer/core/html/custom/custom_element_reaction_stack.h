// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_REACTION_STACK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_REACTION_STACK_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class CustomElementReaction;
class CustomElementReactionQueue;
class Element;

// https://html.spec.whatwg.org/C/#custom-element-reactions
class CORE_EXPORT CustomElementReactionStack final
    : public GarbageCollected<CustomElementReactionStack>,
      public NameClient {
 public:
  CustomElementReactionStack();

  void Trace(Visitor*);
  const char* NameInHeapSnapshot() const override {
    return "CustomElementReactionStack";
  }

  void Push();
  void PopInvokingReactions();
  void EnqueueToCurrentQueue(Element&, CustomElementReaction&);
  void EnqueueToBackupQueue(Element&, CustomElementReaction&);
  void ClearQueue(Element&);

  static CustomElementReactionStack& Current();

 private:
  friend class CustomElementReactionStackTestSupport;

  using ElementReactionQueueMap =
      HeapHashMap<Member<Element>, Member<CustomElementReactionQueue>>;
  ElementReactionQueueMap map_;

  using ElementQueue = HeapVector<Member<Element>, 1>;
  HeapVector<Member<ElementQueue>> stack_;
  Member<ElementQueue> backup_queue_;

  void InvokeBackupQueue();
  void InvokeReactions(ElementQueue&);
  void Enqueue(Member<ElementQueue>&, Element&, CustomElementReaction&);

  DISALLOW_COPY_AND_ASSIGN(CustomElementReactionStack);
};

class CORE_EXPORT CustomElementReactionStackTestSupport final {
 private:
  friend class ResetCustomElementReactionStackForTest;

  CustomElementReactionStackTestSupport() = delete;
  static CustomElementReactionStack* SetCurrentForTest(
      CustomElementReactionStack*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_REACTION_STACK_H_
