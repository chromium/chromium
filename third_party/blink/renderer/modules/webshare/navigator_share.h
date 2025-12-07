// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSHARE_NAVIGATOR_SHARE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSHARE_NAVIGATOR_SHARE_H_

#include "third_party/blink/public/mojom/webshare/webshare.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class ExceptionState;
class Navigator;
class ShareData;

class MODULES_EXPORT NavigatorShare final
    : public GarbageCollected<NavigatorShare>,
      public GarbageCollectedMixin {
 public:
  static const unsigned kSupplementIndex;

  explicit NavigatorShare(Navigator& navigator) : navigator_(navigator) {}
  ~NavigatorShare() = default;

  // Gets, or creates, NavigatorShare supplement on Navigator.
  static NavigatorShare& From(Navigator&);

  // Navigator partial interface
  bool canShare(ScriptState*, const ShareData*);
  static bool canShare(ScriptState*, Navigator&, const ShareData*);
  ScriptPromise<IDLUndefined> share(ScriptState*,
                                    const ShareData*,
                                    ExceptionState&);
  static ScriptPromise<IDLUndefined> share(ScriptState*,
                                           Navigator&,
                                           const ShareData*,
                                           ExceptionState&);

  void Trace(Visitor*) const override;

 private:
  class ShareClientImpl;

  void OnConnectionError();

  Member<Navigator> navigator_;

  // |NavigatorShare| is not ExecutionContext-associated.
  HeapMojoRemote<blink::mojom::blink::ShareService> service_remote_{nullptr};

  // Represents a user's current intent to share some data.
  // This set must have at most 1 element on non-Android platforms. This is a
  // set, and not just and object in order to work around an Android specific
  // bug in opposition to the web-share spec.
  HeapHashSet<Member<ShareClientImpl>> clients_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSHARE_NAVIGATOR_SHARE_H_
