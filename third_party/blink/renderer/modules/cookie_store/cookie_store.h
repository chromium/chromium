// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_STORE_COOKIE_STORE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_STORE_COOKIE_STORE_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/cookie_store/cookie_store.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CanonicalCookie;
class CookieStoreDeleteOptions;
class CookieStoreGetOptions;
class CookieStoreSetOptions;
class CookieStoreSetExtraOptions;
class ScriptPromiseResolver;
class ScriptState;

class CookieStore final : public EventTargetWithInlineData,
                          public ContextLifecycleObserver,
                          public network::mojom::blink::CookieChangeListener {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(CookieStore);

 public:
  CookieStore(
      ExecutionContext*,
      mojo::Remote<network::mojom::blink::RestrictedCookieManager> backend,
      mojo::Remote<blink::mojom::blink::CookieStore> subscription_backend);
  // Needed because of the
  // mojo::Remote<network::mojom::blink::RestrictedCookieManager>
  ~CookieStore() override;

  ScriptPromise getAll(ScriptState*, const String& name, ExceptionState&);
  ScriptPromise getAll(ScriptState*,
                       const CookieStoreGetOptions*,
                       ExceptionState&);
  ScriptPromise get(ScriptState*, const String& name, ExceptionState&);
  ScriptPromise get(ScriptState*,
                    const CookieStoreGetOptions*,
                    ExceptionState&);

  ScriptPromise set(ScriptState*,
                    const CookieStoreSetExtraOptions*,
                    ExceptionState&);
  ScriptPromise set(ScriptState*,
                    const String& name,
                    const String& value,
                    const CookieStoreSetOptions*,
                    ExceptionState&);
  ScriptPromise Delete(ScriptState*, const String& name, ExceptionState&);
  ScriptPromise Delete(ScriptState*,
                       const CookieStoreDeleteOptions*,
                       ExceptionState&);
  ScriptPromise subscribeToChanges(
      ScriptState*,
      const HeapVector<Member<CookieStoreGetOptions>>& subscriptions,
      ExceptionState&);
  ScriptPromise getChangeSubscriptions(ScriptState*, ExceptionState&);

  // GarbageCollected
  void Trace(blink::Visitor* visitor) override {
    EventTargetWithInlineData::Trace(visitor);
    ContextLifecycleObserver::Trace(visitor);
  }

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) override;

  // EventTargetWithInlineData
  DEFINE_ATTRIBUTE_EVENT_LISTENER(change, kChange)
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;
  void RemoveAllEventListeners() override;

  // RestrictedCookieChangeListener
  void OnCookieChange(
      network::mojom::blink::CookieChangeInfoPtr change) override;

 protected:
  // EventTarget overrides.
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) final;
  void RemovedEventListener(const AtomicString& event_type,
                            const RegisteredEventListener&) final;

 private:
  using DoReadBackendResultConverter = void (*)(ScriptPromiseResolver*,
                                                const Vector<CanonicalCookie>&);

  // Common code in CookieStore::{get,getAll}.
  //
  // All cookie-reading methods use the same RestrictedCookieManager API, and
  // only differ in how they present the returned data. The difference is
  // captured in the DoReadBackendResultConverter argument, which should point
  // to one of the static methods below.
  ScriptPromise DoRead(ScriptState*,
                       const CookieStoreGetOptions*,
                       DoReadBackendResultConverter,
                       ExceptionState&);

  // Converts the result of a RestrictedCookieManager::GetAllForUrl mojo call to
  // the promise result expected by CookieStore.getAll.
  static void GetAllForUrlToGetAllResult(
      ScriptPromiseResolver*,
      const Vector<CanonicalCookie>& backend_result);

  // Converts the result of a RestrictedCookieManager::GetAllForUrl mojo call to
  // the promise result expected by CookieStore.get.
  static void GetAllForUrlToGetResult(
      ScriptPromiseResolver*,
      const Vector<CanonicalCookie>& backend_result);

  // Common code in CookieStore::delete and CookieStore::set.
  ScriptPromise DoWrite(ScriptState*,
                        const CookieStoreSetExtraOptions*,
                        ExceptionState&);

  static void OnSetCanonicalCookieResult(ScriptPromiseResolver*,
                                         bool backend_result);

  static void OnSubscribeToCookieChangesResult(ScriptPromiseResolver*,
                                               bool backend_result);
  static void OnGetCookieChangeSubscriptionResult(
      ScriptPromiseResolver*,
      Vector<blink::mojom::blink::CookieChangeSubscriptionPtr> backend_result,
      bool backend_success);

  // Called when a change event listener is added.
  //
  // This is idempotent during the time intervals between StopObserving() calls.
  void StartObserving();

  // Called when all the change event listeners have been removed.
  void StopObserving();

  // Wraps an always-on Mojo pipe for sending requests to the Network Service.
  mojo::Remote<network::mojom::blink::RestrictedCookieManager> backend_;

  // Wraps a Mojo pipe for managing service worker cookie change subscriptions.
  //
  // This pipe is always connected in service worker execution contexts, and
  // never connected in document contexts.
  mojo::Remote<blink::mojom::blink::CookieStore> subscription_backend_;

  // Wraps a Mojo pipe used to receive cookie change notifications.
  //
  // This receiver is set up on-demand, when the cookie store has at least one
  // change event listener. If all the listeners are unregistered, the receiver
  // is torn down.
  mojo::Receiver<network::mojom::blink::CookieChangeListener>
      change_listener_receiver_{this};

  // Default for cookie_url in CookieStoreGetOptions.
  //
  // This is the current document's URL. API calls coming from a document
  // context are not allowed to specify a different cookie_url, whereas Service
  // Workers may specify any URL that falls under their registration.
  const KURL default_cookie_url_;

  // The RFC 6265bis "site for cookies" for this store's ExecutionContext.
  const KURL default_site_for_cookies_;

  // The context in which cookies are accessed.
  const scoped_refptr<SecurityOrigin> default_top_frame_origin_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_STORE_COOKIE_STORE_H_
