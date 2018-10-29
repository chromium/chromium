// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BACKGROUND_FETCH_BACKGROUND_FETCH_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BACKGROUND_FETCH_BACKGROUND_FETCH_MANAGER_H_

#include "base/time/time.h"
#include "third_party/blink/public/platform/modules/background_fetch/background_fetch.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

class SkBitmap;

namespace blink {

class BackgroundFetchBridge;
class BackgroundFetchIconLoader;
class BackgroundFetchOptions;
class BackgroundFetchRegistration;
class ExceptionState;
class ExecutionContext;
class RequestOrUSVStringOrRequestOrUSVStringSequence;
class ScriptPromiseResolver;
class ScriptState;
class ServiceWorkerRegistration;
class WebServiceWorkerRequest;

// Implementation of the BackgroundFetchManager JavaScript object, accessible
// by developers through ServiceWorkerRegistration.backgroundFetch.
class MODULES_EXPORT BackgroundFetchManager final
    : public ScriptWrappable,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(BackgroundFetchManager);
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~BackgroundFetchManager() override = default;
  static BackgroundFetchManager* Create(
      ServiceWorkerRegistration* registration) {
    return new BackgroundFetchManager(registration);
  }

  // Web Exposed methods defined in the IDL file.
  ScriptPromise fetch(
      ScriptState* script_state,
      const String& id,
      const RequestOrUSVStringOrRequestOrUSVStringSequence& requests,
      const BackgroundFetchOptions& options,
      ExceptionState& exception_state);
  ScriptPromise get(ScriptState* script_state, const String& id);
  ScriptPromise getIds(ScriptState* script_state);

  void Trace(blink::Visitor* visitor) override;

  // ContextLifecycleObserver interface
  void ContextDestroyed(ExecutionContext* context) override;

 private:
  friend class BackgroundFetchManagerTest;

  explicit BackgroundFetchManager(ServiceWorkerRegistration* registration);

  // Creates a vector of WebServiceWorkerRequest objects for the given set of
  // |requests|, which can be either Request objects or URL strings.
  // |has_requests_with_body| will be set if any of the |requests| has a body.
  static Vector<WebServiceWorkerRequest> CreateWebRequestVector(
      ScriptState* script_state,
      const RequestOrUSVStringOrRequestOrUSVStringSequence& requests,
      ExceptionState& exception_state,
      bool* has_requests_with_body);

  void DidLoadIcons(const String& id,
                    Vector<WebServiceWorkerRequest> web_requests,
                    mojom::blink::BackgroundFetchOptionsPtr options,
                    ScriptPromiseResolver* resolver,
                    BackgroundFetchIconLoader* loader,
                    const SkBitmap& icon,
                    int64_t ideal_to_chosen_icon_size);
  void DidFetch(ScriptPromiseResolver* resolver,
                base::Time time_started,
                mojom::blink::BackgroundFetchError error,
                BackgroundFetchRegistration* registration);
  void DidGetRegistration(ScriptPromiseResolver* script_state,
                          base::Time time_started,
                          mojom::blink::BackgroundFetchError error,
                          BackgroundFetchRegistration* registration);
  void DidGetDeveloperIds(ScriptPromiseResolver* script_state,
                          base::Time time_started,
                          mojom::blink::BackgroundFetchError error,
                          const Vector<String>& developer_ids);

  Member<ServiceWorkerRegistration> registration_;
  Member<BackgroundFetchBridge> bridge_;
  HeapVector<Member<BackgroundFetchIconLoader>> loaders_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BACKGROUND_FETCH_BACKGROUND_FETCH_MANAGER_H_
