// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_PLATFORM_BINDER_EXCHANGE_H_
#define MOJO_PUBLIC_CPP_PLATFORM_BINDER_EXCHANGE_H_

#include <utility>

#include "base/android/binder.h"
#include "base/component_export.h"
#include "base/functional/callback.h"

namespace mojo {

// Creates a pair of logically entangled BinderRefs which can be used to
// establish two-way communication between arbitrary binder endpoints.
//
// This exists because Binder communication is asymmetrical. To receive
// transactions from a client, an object must generate a transferable reference
// to itself (an IBinder, or base::android::BinderRef in Chromium C++) and pass
// it to the client.
//
// PlatformChannel on the other hand is symmetrical, with a transferable and
// bidirectional PlatformChannelEndpoint at each end. It's common for one
// process to create a PlatformChannel and pass both endpoints to separate
// processes for communication. To model this sort of primitive on Binder, we
// implement a simple exchange protocol facilitated by these functions.
//
// CreateBinderExchange() returns two binders which feed into the same bespoke
// exchange broker in the creating process. These binders can be distributed to
// other processes, and each one can be given to ExchangeBinders() to
// asynchronously exchange an endpoint binder with a remote peer. The exchange
// is not prescriptive about what type of interface is used by each endpoint
// binder, as the binders are opaquely passed through the broker from one client
// to the other.
using BinderPair =
    std::pair<base::android::BinderRef, base::android::BinderRef>;
COMPONENT_EXPORT(MOJO_CPP_PLATFORM) BinderPair CreateBinderExchange();

// Asynchronously completes a binder exchange.
//
// `exchange_binder` is either one of two binders returned by a prior call to
// CreateBinderExchange(), potentially from another process.
//
// `endpoint_binder` is a binder of arbitrary type, referencing an endpoint
// object the caller wishes to exchange for a peer endpoint.
//
// `callback` is a callback which is guaranteed to be called eventually if
// ExchangeBinders() returns base::ok(). It will be called either with the peer
// endpoint's own binder, or with a null binder if exchange was aborted, e.g.,
// due to peer death. It may be called from any thread.
//
// If an error status is returned, `callback` is discarded and never invoked.
using ExchangeBindersCallback =
    base::OnceCallback<void(base::android::BinderRef)>;
COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
base::android::BinderStatusOr<void> ExchangeBinders(
    base::android::BinderRef exchange_binder,
    base::android::BinderRef endpoint_binder,
    ExchangeBindersCallback callback);

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_PLATFORM_BINDER_EXCHANGE_H_
