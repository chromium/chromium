// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/binder_exchange.h"

#include <array>
#include <optional>
#include <utility>

#include "base/android/binder.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace mojo {

namespace {

// Binder class used to identify ExchangeReceiver binders.
DEFINE_BINDER_CLASS(ExchangeInterface);

// Binder class used to identify PeerConnector binders.
DEFINE_BINDER_CLASS(PeerConnectorInterface);

// Expects a single transaction containing an arbitrary endpoint binder, and
// forwards it to a callback. An instance of this is created internally at each
// endpoint performing a binder exchange. Its only job is to receive a peer
// endpoint from the shared Exchange instance and pass it along to the local
// endpoint, completing one side of the exchange.
class PeerConnector
    : public base::android::SupportsBinder<PeerConnectorInterface> {
 public:
  using Proxy = PeerConnectorInterface::BinderRef;

  // Transaction code for ConnectToPeer().
  static constexpr transaction_code_t kConnectToPeer = 1;

  // `callback` is the local endpoint's callback, which will be invoked with the
  // peer's binder as soon as it's received; or with a null binder if we're
  // destroyed first.
  explicit PeerConnector(ExchangeBindersCallback callback)
      : callback_(std::move(callback)) {}

  // Sends an endpoint binder to a remote PeerConnector instance via `proxy`.
  // The message is received in OnBinderTransaction() below.
  static base::android::BinderStatusOr<void> ConnectToPeer(
      Proxy& proxy,
      base::android::BinderRef peer_endpoint_binder) {
    ASSIGN_OR_RETURN(auto parcel, proxy.PrepareTransaction());
    RETURN_IF_ERROR(
        parcel.writer().WriteBinder(std::move(peer_endpoint_binder)));
    return proxy.TransactOneWay(kConnectToPeer, std::move(parcel));
  }

  // Drops this connector's callback without invocation.
  void Cancel() { std::ignore = TakeCallback(); }

 private:
  ~PeerConnector() override {
    if (callback_) {
      // If our binder loses all references and we're destroyed without invoking
      // the callback, invoke with a null binder to convey failure. The callback
      // is required to be safe to call from any thread.
      std::move(callback_).Run(base::android::BinderRef());
    }
  }

  ExchangeBindersCallback TakeCallback() {
    base::AutoLock lock(lock_);
    // Note that moved-from callbacks are guaranteed to be null after the move.
    return std::move(callback_);
  }

  // base::android::SupportsBinder<PeerConnectorInterface>:
  base::android::BinderStatusOr<void> OnBinderTransaction(
      transaction_code_t code,
      const base::android::ParcelReader& in,
      const base::android::ParcelWriter& out) override {
    if (code != kConnectToPeer) {
      return base::unexpected(STATUS_UNKNOWN_TRANSACTION);
    }

    ASSIGN_OR_RETURN(auto peer_endpoint_binder, in.ReadBinder());

    ExchangeBindersCallback callback = TakeCallback();
    if (!callback) {
      // If the callback is null then we've already received a peer binder and
      // invoked the callback with it.
      return base::unexpected(STATUS_ALREADY_EXISTS);
    }

    std::move(callback).Run(std::move(peer_endpoint_binder));
    return base::ok();
  }

  base::Lock lock_;
  ExchangeBindersCallback callback_ GUARDED_BY(lock_);
};

// A broker which mediates binder exchange between two endpoints.
class Exchange : public base::RefCountedThreadSafe<Exchange> {
 public:
  Exchange() = default;

  // Called by each of two ExchangeReceivers when they receive an endpoint
  // binder. Well behaved clients will only call this once, but we still need to
  // deal with multiple calls safely. `which` is always 0 or 1 and does not come
  // from the client. See CreateBinderExchange() below.
  base::android::BinderStatusOr<void> AcceptEndpoint(
      int which,
      base::android::BinderRef binder,
      PeerConnector::Proxy connector) {
    ReadyEndpoint ready_endpoint = {std::move(binder), std::move(connector)};
    std::optional<ReadyEndpoint> first, second;
    {
      base::AutoLock lock(lock_);
      auto& ours = endpoints_[which];
      if (!absl::holds_alternative<NoEndpoint>(ours)) {
        // There's no situation where we expect to receive a binder for this
        // endpoint once its state has already changed in some way. Consider
        // this to be a validation failure.
        ours = DeadEndpoint{};
        return base::unexpected(STATUS_ALREADY_EXISTS);
      }

      auto& theirs = endpoints_[1 - which];
      if (absl::holds_alternative<DeadEndpoint>(theirs)) {
        // The peer is already gone. We can silently drop this endpoint too.
        ours = DeadEndpoint{};
        return base::ok();
      }

      ours = std::move(ready_endpoint);
      if (absl::holds_alternative<NoEndpoint>(theirs)) {
        // Still waiting for the peer endpoint. The next time AcceptEndpoint()
        // is called it should be for the peer's endpoint, and at that point
        // they will discover that ours is ready too and proceed below.
        return base::ok();
      }

      // If we're here, both endpoints are now ready to be exchanged.
      first =
          absl::get<ReadyEndpoint>(std::exchange(ours, ExchangedEndpoint()));
      second =
          absl::get<ReadyEndpoint>(std::exchange(theirs, ExchangedEndpoint()));
    }

    RETURN_IF_ERROR(PeerConnector::ConnectToPeer(first->connector,
                                                 std::move(second->binder)));
    RETURN_IF_ERROR(PeerConnector::ConnectToPeer(second->connector,
                                                 std::move(first->binder)));
    return base::ok();
  }

  void NotifyEndpointDisconnected(int which) {
    // Disconnection is only interesting if it happens before we've received a
    // binder for this endpoint.
    base::AutoLock lock(lock_);
    auto& ours = endpoints_[which];
    if (absl::holds_alternative<NoEndpoint>(ours)) {
      ours = DeadEndpoint{};

      // If the peer endpoint was ready, disconnect it too. Otherwise the peer's
      // state doesn't need to change: either it's already disconnected too or
      // it hasn't been received yet and will be dropped once it arrives.
      auto& theirs = endpoints_[1 - which];
      if (absl::holds_alternative<ReadyEndpoint>(theirs)) {
        theirs = DeadEndpoint{};
      }
    }
  }

 private:
  friend class base::RefCountedThreadSafe<Exchange>;

  // An endpoint which hasn't arrived yet.
  struct NoEndpoint {};

  // An endpoint which has arrived and is waiting for its peer.
  struct ReadyEndpoint {
    base::android::BinderRef binder;
    PeerConnector::Proxy connector;
  };

  // An endpoint which was disconnected without providing a binder to itself.
  struct DeadEndpoint {};

  // An endpoint which arrived and has already been exchanged with its peer.
  struct ExchangedEndpoint {};

  using Endpoint =
      absl::variant<NoEndpoint, ReadyEndpoint, DeadEndpoint, ExchangedEndpoint>;

  ~Exchange() = default;

  base::Lock lock_;

  // Tracks the state of both endpoints in this exchange.
  std::array<Endpoint, 2> endpoints_ GUARDED_BY(lock_) = {NoEndpoint{},
                                                          NoEndpoint{}};
};

// Expects a single transaction containing two binders: one to an arbitrary
// endpoint object and one to a PeerConnector. These are forwarded to a local
// Exchange instance shared by exactly one other local ExchangeReceiver.
class ExchangeReceiver
    : public base::android::SupportsBinder<ExchangeInterface> {
 public:
  using Proxy = ExchangeInterface::BinderRef;

  // Transaction code for ExchangeBinders().
  static constexpr transaction_code_t kExchangeBinders = 1;

  // `which` is 0 or 1, identifying one of two endpoints for the Exchange. This
  // receiver exclusively controls that endpoint on the Exchange.
  ExchangeReceiver(scoped_refptr<Exchange> exchange, int which)
      : exchange_(std::move(exchange)), which_(which) {}

  // Sends an endpoint binder and a PeerConnector binder to a remote
  // ExchangeReceiver instance via `proxy`. This message is received in
  // OnBinderTransaction() below.
  static base::android::BinderStatusOr<void> ExchangeBinders(
      Proxy& proxy,
      base::android::BinderRef endpoint_binder,
      base::android::BinderRef connector_binder) {
    ASSIGN_OR_RETURN(auto parcel, proxy.PrepareTransaction());
    RETURN_IF_ERROR(parcel.writer().WriteBinder(std::move(endpoint_binder)));
    RETURN_IF_ERROR(parcel.writer().WriteBinder(std::move(connector_binder)));
    return proxy.TransactOneWay(kExchangeBinders, std::move(parcel));
  }

 private:
  ~ExchangeReceiver() override = default;

  // base::android::SupportsBinder<ExchangeInterface>:
  base::android::BinderStatusOr<void> OnBinderTransaction(
      transaction_code_t code,
      const base::android::ParcelReader& in,
      const base::android::ParcelWriter& out) override {
    if (code != kExchangeBinders) {
      return base::unexpected(STATUS_UNKNOWN_TRANSACTION);
    }
    ASSIGN_OR_RETURN(auto endpoint_binder, in.ReadBinder());
    ASSIGN_OR_RETURN(auto connector_binder, in.ReadBinder());
    auto connector = PeerConnector::Proxy::Adopt(std::move(connector_binder));
    if (!connector) {
      return base::unexpected(STATUS_BAD_TYPE);
    }
    return exchange_->AcceptEndpoint(which_, std::move(endpoint_binder),
                                     std::move(connector));
  }

  void OnBinderDestroyed() override {
    exchange_->NotifyEndpointDisconnected(which_);
  }

  const scoped_refptr<Exchange> exchange_;
  const int which_;
};

}  // namespace

BinderPair CreateBinderExchange() {
  auto exchange = base::MakeRefCounted<Exchange>();
  auto receiver0 = base::MakeRefCounted<ExchangeReceiver>(exchange, 0);
  auto receiver1 =
      base::MakeRefCounted<ExchangeReceiver>(std::move(exchange), 1);
  return {receiver0->GetBinder(), receiver1->GetBinder()};
}

base::android::BinderStatusOr<void> ExchangeBinders(
    base::android::BinderRef exchange_binder,
    base::android::BinderRef endpoint_binder,
    ExchangeBindersCallback callback) {
  CHECK(callback);
  auto exchange = ExchangeReceiver::Proxy::Adopt(std::move(exchange_binder));
  if (!exchange || !endpoint_binder) {
    return base::unexpected(STATUS_BAD_TYPE);
  }
  auto connector = base::MakeRefCounted<PeerConnector>(std::move(callback));
  auto result = ExchangeReceiver::ExchangeBinders(
      exchange, std::move(endpoint_binder), connector->GetBinder());
  if (!result.has_value()) {
    // We're returning an error, so we must not allow the callback to be called.
    connector->Cancel();
  }
  return result;
}

}  // namespace mojo
