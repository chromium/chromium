// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_HTTP_TRANSACTION_FACTORY_OVERRIDE_H_
#define NET_URL_REQUEST_URL_REQUEST_HTTP_TRANSACTION_FACTORY_OVERRIDE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/supports_user_data.h"
#include "net/base/net_export.h"

namespace net {

class HttpTransactionFactory;
class URLRequest;

// A ref-counted handle to an HttpTransactionFactory. Stored in both the
// owner of the factory (e.g., NetworkContext::IsolatedSession) and the
// HttpTransactionFactoryOverride on each URLRequest. The handle is cleared
// (factory set to null) when the owner destroys the session. Because
// URLRequestHttpJob consults the override only when it creates a new
// HttpTransaction, clearing it affects *subsequent* transaction creations
// on an in-flight request (auth or redirect restarts), which fall back to
// the default factory; an HttpTransaction that already exists keeps running
// on the session it was created from.
class NET_EXPORT HttpTransactionFactoryHandle
    : public base::RefCounted<HttpTransactionFactoryHandle> {
 public:
  // |factory| is the transaction factory to use. |destruction_callback| is
  // a type-erased guard that keeps the factory's owner alive as long as any
  // handle reference exists (via its bound state, e.g. a
  // scoped_refptr<TheOwner>), and is run when the last handle reference is
  // released - letting the owner reclaim per-session state promptly.
  // |disables_network_quality_estimation| must be set when the factory
  // simulates network conditions (e.g. socket-layer throttling): requests
  // routed through it are then excluded from NetworkQualityEstimator
  // observations, so simulated RTT/throughput cannot contaminate the global
  // network-quality estimate. It is an explicit property rather than being
  // inferred from the override's presence, because this override mechanism
  // is not inherently tied to emulation.
  HttpTransactionFactoryHandle(HttpTransactionFactory* factory,
                               base::OnceClosure destruction_callback,
                               bool disables_network_quality_estimation);

  HttpTransactionFactory* factory() const { return factory_; }

  bool disables_network_quality_estimation() const {
    return disables_network_quality_estimation_;
  }

  // Called by the owner when the factory should no longer be used. The next
  // transaction an in-flight request creates will see nullptr and fall back
  // to the default factory; a transaction it already created keeps running.
  //
  // `destruction_callback_` is deliberately NOT released here. It carries the
  // guard that keeps the factory's owner alive, and requests that already
  // created a transaction are still using that owner's session. Dropping the
  // guard at Reset() time would free the session underneath them - in
  // ~NetworkContext, for instance, Reset() runs while `url_loader_factories_`
  // (and therefore the URLRequests holding these handles) are still alive and
  // are only destroyed further down that destructor. The guard is released
  // when the last handle reference goes away, which is the point at which no
  // request can be using the session any more.
  void Reset() { factory_ = nullptr; }

 private:
  friend class base::RefCounted<HttpTransactionFactoryHandle>;
  ~HttpTransactionFactoryHandle();

  raw_ptr<HttpTransactionFactory> factory_;
  base::OnceClosure destruction_callback_;
  const bool disables_network_quality_estimation_;
};

// Attaches an alternate HttpTransactionFactory to a URLRequest via
// SupportsUserData. When present, URLRequestHttpJob uses this factory
// instead of the URLRequestContext's default factory to create the
// HTTP transaction. This enables per-request routing to isolated
// HttpNetworkSessions (e.g., for per-tab socket pool isolation).
class NET_EXPORT HttpTransactionFactoryOverride
    : public base::SupportsUserData::Data {
 public:
  explicit HttpTransactionFactoryOverride(
      scoped_refptr<HttpTransactionFactoryHandle> handle);
  ~HttpTransactionFactoryOverride() override;

  // Returns the factory, or nullptr if the session was destroyed.
  HttpTransactionFactory* factory() const {
    return handle_ ? handle_->factory() : nullptr;
  }

  // Sets the override on a URLRequest.
  static void Set(URLRequest* request,
                  scoped_refptr<HttpTransactionFactoryHandle> handle);

  // Gets the factory from a URLRequest, or nullptr if not set or session gone.
  static HttpTransactionFactory* Get(const URLRequest* request);

  // True iff an override is attached to |request| whose handle disables
  // network quality estimation (see HttpTransactionFactoryHandle). Unlike
  // Get(), this remains true after the handle is Reset(): a request that ran
  // under simulated conditions must never feed the estimator.
  static bool DisablesNetworkQualityEstimation(const URLRequest* request);

 private:
  static const void* const kKey;
  scoped_refptr<HttpTransactionFactoryHandle> handle_;
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_HTTP_TRANSACTION_FACTORY_OVERRIDE_H_
