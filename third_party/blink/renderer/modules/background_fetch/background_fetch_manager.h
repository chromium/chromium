// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BACKGROUND_FETCH_BACKGROUND_FETCH_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BACKGROUND_FETCH_BACKGROUND_FETCH_MANAGER_H_

#include "base/time/time.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

class SkBitmap;

namespace blink {

class BackgroundFetchBridge;
class BackgroundFetchIconLoader;
class BackgroundFetchOptions;
class BackgroundFetchRegistration;
class ExceptionState;
class ScriptPromiseResolver;
class ScriptState;
class ServiceWorkerRegistration;

// Implementation of the BackgroundFetchManager JavaScript object, accessible
// by developers through ServiceWorkerRegistration.backgroundFetch.
class MODULES_EXPORT BackgroundFetchManager final
    : public ScriptWrappable,
      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit BackgroundFetchManager(ServiceWorkerRegistration* registration);
  ~BackgroundFetchManager() override = default;

  // Web Exposed methods defined in the IDL file.
  ScriptPromise fetch(
      ScriptState* script_state,
      const String& id,
      const V8UnionRequestInfoOrRequestOrUSVStringSequence* requests,
      const BackgroundFetchOptions* options,
      ExceptionState& exception_state);
  ScriptPromise get(ScriptState* script_state,
                    const String& id,
                    ExceptionState& exception_state);
  ScriptPromise getIds(ScriptState* script_state,
                       ExceptionState& exception_state);

  void Trace(Visitor* visitor) const override;

  // ExecutionContextLifecycleObserver interface
  void ContextDestroyed() override;

 private:
  friend class BackgroundFetchManagerTest;

  // Creates a vector of mojom::blink::FetchAPIRequestPtr objects for the given
  // set of |requests|, which can be either Request objects or URL strings.
  // |has_requests_with_body| will be set if any of the |requests| has a body.
  static Vector<mojom::blink::FetchAPIRequestPtr> CreateFetchAPIRequestVector(
      ScriptState* script_state,
      const V8UnionRequestInfoOrRequestOrUSVStringSequence* requests,
      ExceptionState& exception_state,
      bool* has_requests_with_body);

  void DidLoadIcons(const String& id,
                    Vector<mojom::blink::FetchAPIRequestPtr> requests,
                    mojom::blink::BackgroundFetchOptionsPtr options,
                    BackgroundFetchIconLoader* loader,
                    ScriptPromiseResolver* resolver,
                    const SkBitmap& icon,
                    int64_t ideal_to_chosen_icon_size);
  void DidFetch(base::Time time_started,
                ScriptPromiseResolver* resolver,
                mojom::blink::BackgroundFetchError error,
                BackgroundFetchRegistration* registration);
  void DidGetRegistration(base::Time time_started,
                          ScriptPromiseResolver* resolver,
                          mojom::blink::BackgroundFetchError error,
                          BackgroundFetchRegistration* registration);
  void DidGetDeveloperIds(base::Time time_started,
                          ScriptPromiseResolver* resolver,
                          mojom::blink::BackgroundFetchError error,
                          const Vector<String>& developer_ids);

  Member<ServiceWorkerRegistration> registration_;
  Member<BackgroundFetchBridge> bridge_;
  HeapVector<Member<BackgroundFetchIconLoader>> loaders_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BACKGROUND_FETCH_BACKGROUND_FETCH_MANAGER_H_
