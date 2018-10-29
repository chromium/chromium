// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/platform/modules/background_fetch/background_fetch.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CacheQueryOptions;
class ScriptPromiseResolver;
class ScriptState;
class ServiceWorkerRegistration;
class RequestOrUSVString;
struct WebBackgroundFetchRegistration;

// Represents an individual Background Fetch registration. Gives developers
// access to its properties, options, and enables them to abort the fetch.
class BackgroundFetchRegistration final
    : public EventTargetWithInlineData,
      public blink::mojom::blink::BackgroundFetchRegistrationObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(BackgroundFetchRegistration, Dispose);

 public:
  BackgroundFetchRegistration(
      const String& developer_id,
      const String& unique_id,
      unsigned long long upload_total,
      unsigned long long uploaded,
      unsigned long long download_total,
      unsigned long long downloaded,
      mojom::BackgroundFetchResult result,
      mojom::BackgroundFetchFailureReason failure_reason);

  BackgroundFetchRegistration(
      ServiceWorkerRegistration* registration,
      const WebBackgroundFetchRegistration& web_registration);

  ~BackgroundFetchRegistration() override;

  // Initializes the BackgroundFetchRegistration to be associated with the given
  // ServiceWorkerRegistration. It will register itself as an observer for
  // progress events, powering the `progress` JavaScript event.
  void Initialize(ServiceWorkerRegistration* registration);

  // BackgroundFetchRegistrationObserver implementation.
  void OnProgress(uint64_t upload_total,
                  uint64_t uploaded,
                  uint64_t download_total,
                  uint64_t downloaded,
                  mojom::BackgroundFetchResult result,
                  mojom::BackgroundFetchFailureReason failure_reason) override;
  void OnRecordsUnavailable() override;

  // Web Exposed attribute defined in the IDL file. Corresponds to the
  // |developer_id| used elsewhere in the codebase.
  String id() const;
  ScriptPromise match(ScriptState* script_state,
                      const RequestOrUSVString& request,
                      const CacheQueryOptions& options,
                      ExceptionState& exception_state);
  ScriptPromise matchAll(ScriptState* scrip_state,
                         ExceptionState& exception_state);
  ScriptPromise matchAll(ScriptState* script_state,
                         const RequestOrUSVString& request,
                         const CacheQueryOptions& options,
                         ExceptionState& exception_state);

  unsigned long long uploadTotal() const;
  unsigned long long uploaded() const;
  unsigned long long downloadTotal() const;
  unsigned long long downloaded() const;
  bool recordsAvailable() const;
  const String result() const;
  const String failureReason() const;

  const String& unique_id() const { return unique_id_; }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(progress);

  ScriptPromise abort(ScriptState* script_state);

  // EventTargetWithInlineData implementation.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  void Dispose();

  void Trace(blink::Visitor* visitor) override;

 private:
  void DidAbort(ScriptPromiseResolver* resolver,
                mojom::blink::BackgroundFetchError error);
  ScriptPromise MatchImpl(ScriptState* script_state,
                          base::Optional<RequestOrUSVString> request,
                          mojom::blink::QueryParamsPtr cache_query_params,
                          ExceptionState& exception_state,
                          bool match_all);
  void DidGetMatchingRequests(
      ScriptPromiseResolver* resolver,
      bool return_all,
      Vector<mojom::blink::BackgroundFetchSettledFetchPtr> settled_fetches);

  Member<ServiceWorkerRegistration> registration_;

  // Corresponds to IDL 'id' attribute. Not unique - an active registration can
  // have the same |developer_id_| as one or more inactive registrations.
  String developer_id_;

  // Globally unique ID for the registration, generated in content/. Used to
  // distinguish registrations in case a developer re-uses |developer_id_|s. Not
  // exposed to JavaScript.
  String unique_id_;

  unsigned long long upload_total_;
  unsigned long long uploaded_;
  unsigned long long download_total_;
  unsigned long long downloaded_;
  bool records_available_ = true;
  mojom::BackgroundFetchResult result_;
  mojom::BackgroundFetchFailureReason failure_reason_;

  mojo::Binding<blink::mojom::blink::BackgroundFetchRegistrationObserver>
      observer_binding_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_H_
