/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FULLSCREEN_FULLSCREEN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FULLSCREEN_FULLSCREEN_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class FullscreenOptions;
class ScriptPromiseResolver;

// The Fullscreen class implements most of the Fullscreen API Standard,
// https://fullscreen.spec.whatwg.org/, especially its algorithms. It is a
// Document supplement as each document has some fullscreen state, and to
// actually enter and exit fullscreen it (indirectly) uses FullscreenController.
class CORE_EXPORT Fullscreen final : public GarbageCollected<Fullscreen>,
                                     public Supplement<Document>,
                                     public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(Fullscreen);

 public:
  static const char kSupplementName[];

  explicit Fullscreen(Document&);
  virtual ~Fullscreen();

  static Fullscreen& From(Document&);
  static Element* FullscreenElementFrom(Document&);
  static Element* FullscreenElementForBindingFrom(TreeScope&);
  static bool IsFullscreenElement(const Element&);
  static bool IsInFullscreenElementStack(const Element&);

  enum class RequestType {
    // Element.requestFullscreen()
    kUnprefixed,
    // Element.webkitRequestFullscreen()/webkitRequestFullScreen() and
    // HTMLVideoElement.webkitEnterFullscreen()/webkitEnterFullScreen()
    kPrefixed,
    // For WebRemoteFrameImpl to notify that a cross-process descendant frame
    // has requested and is about to enter fullscreen.
    kPrefixedForCrossProcessDescendant,
  };

  static void RequestFullscreen(Element&);
  static ScriptPromise RequestFullscreen(Element&,
                                         const FullscreenOptions*,
                                         RequestType,
                                         ScriptState* state = nullptr);

  static void FullyExitFullscreen(Document&, bool ua_originated = false);
  static ScriptPromise ExitFullscreen(Document&,
                                      ScriptState* state = nullptr,
                                      bool ua_originated = false);

  static bool FullscreenEnabled(Document&);

  // Called by FullscreenController to notify that we've entered or exited
  // fullscreen. All frames are notified, so there may be no pending request.
  static void DidEnterFullscreen(Document&);
  static void DidExitFullscreen(Document&);

  static void DidUpdateSize(Element&);

  static void ElementRemoved(Element&);

  // ContextLifecycleObserver:
  void ContextDestroyed(ExecutionContext*) override;

  void Trace(blink::Visitor*) override;

 private:
  static Fullscreen* FromIfExists(Document&);

  Document* GetDocument();

  static void ContinueRequestFullscreen(Document&,
                                        Element&,
                                        RequestType,
                                        ScriptPromiseResolver* resolver,
                                        bool error);

  static void ContinueExitFullscreen(Document*,
                                     ScriptPromiseResolver* resolver,
                                     bool resize);

  void FullscreenElementChanged(Element* old_element,
                                Element* new_element,
                                RequestType new_request_type);

  // Stores the pending request, promise and the type for executing
  // the asynchronous portion of the request.
  class PendingRequest : public GarbageCollected<PendingRequest> {
   public:
    PendingRequest(Element* element,
                   RequestType type,
                   ScriptPromiseResolver* resolver);
    virtual ~PendingRequest();
    virtual void Trace(blink::Visitor* visitor);

    Element* element() { return element_; }
    RequestType type() { return type_; }
    ScriptPromiseResolver* resolver() { return resolver_; }

   private:
    Member<Element> element_;
    RequestType type_;
    Member<ScriptPromiseResolver> resolver_;

    DISALLOW_COPY_AND_ASSIGN(PendingRequest);
  };
  using PendingRequests = HeapVector<Member<PendingRequest>>;
  PendingRequests pending_requests_;

  using PendingExit = ScriptPromiseResolver;
  using PendingExits = HeapVector<Member<PendingExit>>;
  PendingExits pending_exits_;
};

inline bool Fullscreen::IsFullscreenElement(const Element& element) {
  return FullscreenElementFrom(element.GetDocument()) == &element;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FULLSCREEN_FULLSCREEN_H_
