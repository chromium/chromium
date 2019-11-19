// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSHARE_NAVIGATOR_SHARE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSHARE_NAVIGATOR_SHARE_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class Navigator;
class ShareData;

class MODULES_EXPORT NavigatorShare final
    : public GarbageCollected<NavigatorShare>,
      public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorShare);

 public:
  static const char kSupplementName[];

  NavigatorShare();
  ~NavigatorShare();

  // Gets, or creates, NavigatorShare supplement on Navigator.
  // See platform/Supplementable.h
  static NavigatorShare& From(Navigator&);

  // Navigator partial interface
  bool canShare(ScriptState*, const ShareData*);
  static bool canShare(ScriptState*, Navigator&, const ShareData*);
  ScriptPromise share(ScriptState*, const ShareData*);
  static ScriptPromise share(ScriptState*, Navigator&, const ShareData*);

  void Trace(blink::Visitor*) override;

 private:
  class ShareClientImpl;

  void OnConnectionError();

  mojo::Remote<blink::mojom::blink::ShareService> service_remote_;

  HeapHashSet<Member<ShareClientImpl>> clients_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSHARE_NAVIGATOR_SHARE_H_
