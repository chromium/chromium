// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SELF_DELETING_URL_LOADER_FACTORY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SELF_DELETING_URL_LOADER_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {

// A base class for URLLoaderFactory implementations that takes care of the
// managing the lifetime of the URLLoaderFactory implementation
// which should be owned by the set of its receivers.
class COMPONENT_EXPORT(NETWORK_CPP) SelfDeletingURLLoaderFactory
    : public mojom::URLLoaderFactory {
 public:
  SelfDeletingURLLoaderFactory(const SelfDeletingURLLoaderFactory&) = delete;
  SelfDeletingURLLoaderFactory& operator=(const SelfDeletingURLLoaderFactory&) =
      delete;

 protected:
  // Constructs SelfDeletingURLLoaderFactory object that will self-delete
  // once all receivers disconnect (including |factory_receiver| below as well
  // as receivers that connect via the Clone method).
  explicit SelfDeletingURLLoaderFactory(
      mojo::PendingReceiver<mojom::URLLoaderFactory> factory_receiver);

  ~SelfDeletingURLLoaderFactory() override;

  // The override below is marked as |final| to make sure derived classes do not
  // accidentally side-step lifetime management.
  void Clone(mojo::PendingReceiver<mojom::URLLoaderFactory> loader) final;

  // Sometimes a derived class can no longer function, even when the set of
  // |receivers_| is still non-empty.  This should be rare (typically the
  // lifetime of users of mojo::Remote<mojom::URLLoaderFactory> should
  // be shorter than whatever the factory depends on), but may happen in some
  // corner cases (e.g. in a race between 1) BrowserContext destruction and 2)
  // CreateLoaderAndStart mojo call).
  //
  // When a derived class gets notified that its dependencies got destroyed, it
  // should call DisconnectReceiversAndDestroy to prevent any future calls to
  // CreateLoaderAndStart.
  void DisconnectReceiversAndDestroy();

  // Reports the currently dispatching Message as bad and closes+removes the
  // receiver which received the message. Prefer this over the global
  // `mojo::ReportBadMessage()` function, since calling this method promptly
  // disconnects the receiver, preventing further (potentially bad) messages
  // from being processed.
  NOT_TAIL_CALLED void ReportBadMessage(const std::string& message);

  THREAD_CHECKER(thread_checker_);

 private:
  void OnDisconnect();

  mojo::ReceiverSet<mojom::URLLoaderFactory> receivers_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SELF_DELETING_URL_LOADER_FACTORY_H_
