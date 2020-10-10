// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NON_NETWORK_URL_LOADER_FACTORY_BASE_H_
#define CONTENT_PUBLIC_BROWSER_NON_NETWORK_URL_LOADER_FACTORY_BASE_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

// A base class for URLLoaderFactory implementations that takes care of the
// following aspects that most implementations need to take care of:
//
// - Managing the lifetime of the URLLoaderFactory implementation
//   (owned by the set of its receivers).  See: the final Clone override,
//   OnDisconnect and |receivers_|.
//
// - TODO(toyoshim, lukasza): https://crbug.com/1105256: Checking CORS before
//   handing off to the derived class.
//
// - TODO(lukasza): Responding with file contents: duplicated across
//   FileURLLoaderFactory, ContentURLLoaderFactory (with some additional
//   code sharing probably possible with ExtensionURLLoaderFactory).
class CONTENT_EXPORT NonNetworkURLLoaderFactoryBase
    : public network::mojom::URLLoaderFactory {
 protected:
  // Constructs NonNetworkURLLoaderFactoryBase object that will self-delete once
  // all receivers disconnect (including |factory_receiver| below as well as
  // receivers that connect via the Clone method).
  explicit NonNetworkURLLoaderFactoryBase(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver);

  ~NonNetworkURLLoaderFactoryBase() override;

  // Sometimes a derived class can no longer function, even when the set of
  // |receivers_| is still non-empty.  This should be rare (typically the
  // lifetime of users of mojo::Remote<network::mojom::URLLoaderFactory> should
  // be shorter than whatever the factory depends on), but may happen in some
  // corner cases (e.g. in a race between 1) BrowserContext destruction and 2)
  // CreateLoaderAndStart mojo call).
  //
  // When a derived class gets notified that its dependencies got destroyed, it
  // should call DisconnectReceiversAndDestroy to prevent any future calls to
  // CreateLoaderAndStart.
  void DisconnectReceiversAndDestroy();

  THREAD_CHECKER(thread_checker_);

 private:
  // The override below is marked as |final| to make sure derived classes do not
  // accidentally side-step lifetime management.
  void Clone(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader) final;

  void OnDisconnect();

  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;

  DISALLOW_COPY_AND_ASSIGN(NonNetworkURLLoaderFactoryBase);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NON_NETWORK_URL_LOADER_FACTORY_BASE_H_
