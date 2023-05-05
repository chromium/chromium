// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_STORE_COOKIE_STORE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_STORE_COOKIE_STORE_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/site_for_cookies.h"
#include "services/network/public/mojom/cookie_manager.mojom-blink.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/cookie_store/cookie_store.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CookieInit;
class CookieStoreDeleteOptions;
class CookieStoreGetOptions;
class ExceptionState;
class ScriptPromiseResolver;
class ScriptState;

class CookieStore final : public EventTargetWithInlineData,
                          public ExecutionContextClient,
                          public network::mojom::blink::CookieChangeListener {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CookieStore(
      ExecutionContext*,
      HeapMojoRemote<network::mojom::blink::RestrictedCookieManager> backend);
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
                    const String& name,
                    const String& value,
                    ExceptionState&);
  ScriptPromise set(ScriptState*, const CookieInit*, ExceptionState&);
  ScriptPromise Delete(ScriptState*, const String& name, ExceptionState&);
  ScriptPromise Delete(ScriptState*,
                       const CookieStoreDeleteOptions*,
                       ExceptionState&);

  // GarbageCollected
  void Trace(Visitor* visitor) const override;

  // EventTargetWithInlineData
  DEFINE_ATTRIBUTE_EVENT_LISTENER(change, kChange)
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;
  void RemoveAllEventListeners() override;

  // network::mojom::blink::CookieChangeListener
  void OnCookieChange(
      network::mojom::blink::CookieChangeInfoPtr change) override;

 protected:
  // EventTarget overrides.
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) final;
  void RemovedEventListener(const AtomicString& event_type,
                            const RegisteredEventListener&) final;

 private:
  using DoReadBackendResultConverter =
      void (*)(ScriptPromiseResolver*,
               const Vector<network::mojom::blink::CookieWithAccessResultPtr>);

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
      const Vector<network::mojom::blink::CookieWithAccessResultPtr>
          backend_result);

  // Converts the result of a RestrictedCookieManager::GetAllForUrl mojo call to
  // the promise result expected by CookieStore.get.
  static void GetAllForUrlToGetResult(
      ScriptPromiseResolver*,
      const Vector<network::mojom::blink::CookieWithAccessResultPtr>
          backend_result);

  // Common code in CookieStore::delete and CookieStore::set.
  ScriptPromise DoWrite(ScriptState*, const CookieInit*, ExceptionState&);

  static void OnSetCanonicalCookieResult(ScriptPromiseResolver*,
                                         bool backend_result);

  // Called when a change event listener is added.
  //
  // This is idempotent during the time intervals between StopObserving() calls.
  void StartObserving();

  // Called when all the change event listeners have been removed.
  void StopObserving();

  // Wraps an always-on Mojo pipe for sending requests to the Network Service.
  HeapMojoRemote<network::mojom::blink::RestrictedCookieManager> backend_;

  // Wraps a Mojo pipe used to receive cookie change notifications.
  //
  // This receiver is set up on-demand, when the cookie store has at least one
  // change event listener. If all the listeners are unregistered, the receiver
  // is torn down.
  HeapMojoReceiver<network::mojom::blink::CookieChangeListener, CookieStore>
      change_listener_receiver_;

  // Default for cookie_url in CookieStoreGetOptions.
  //
  // This is the current document's URL. API calls coming from a document
  // context are not allowed to specify a different cookie_url, whereas Service
  // Workers may specify any URL that falls under their registration.
  const KURL default_cookie_url_;

  // The RFC 6265bis "site for cookies" for this store's ExecutionContext.
  const net::SiteForCookies default_site_for_cookies_;

  // The context in which cookies are accessed.
  const scoped_refptr<const SecurityOrigin> default_top_frame_origin_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_STORE_COOKIE_STORE_H_
