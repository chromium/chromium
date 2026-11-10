// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SELF_DELETING_URL_LOADER_FACTORY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SELF_DELETING_URL_LOADER_FACTORY_H_

#include <string_view>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/self_deleting.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {

// A base class for URLLoaderFactory implementations that takes care of the
// managing the lifetime of the URLLoaderFactory implementation
// which should be owned by the set of its receivers.
class COMPONENT_EXPORT(NETWORK_CPP) SelfDeletingURLLoaderFactory
    : public mojom::URLLoaderFactory,
      public base::SelfDeleting {
 public:
  SelfDeletingURLLoaderFactory(const SelfDeletingURLLoaderFactory&) = delete;
  SelfDeletingURLLoaderFactory& operator=(const SelfDeletingURLLoaderFactory&) =
      delete;

 protected:
  // Constructs SelfDeletingURLLoaderFactory object that will self-delete
  // once all receivers disconnect (including |factory_receiver| below as well
  // as receivers that connect via the Clone method).
  //
  // `receiver_task_runner`, if non-null, must run tasks on the sequence that
  // constructs this factory (see mojo::ReceiverSet::Add). It only selects which
  // task queue on that sequence dispatches incoming messages (e.g. a
  // higher-priority queue on the UI thread); it never moves the factory to
  // another thread, so all methods are still invoked on the constructing
  // sequence. Receivers added via Clone() are bound to the same task runner.
  SelfDeletingURLLoaderFactory(
      mojo::PendingReceiver<mojom::URLLoaderFactory> factory_receiver,
      base::SelfDeletingPassKey key,
      scoped_refptr<base::SequencedTaskRunner> receiver_task_runner = nullptr);

  ~SelfDeletingURLLoaderFactory() override;

  // The override below is marked as |final| to make sure derived classes do not
  // accidentally side-step lifetime management. Cloned receivers are bound on
  // `receiver_task_runner_` as well.
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
  NOT_TAIL_CALLED void ReportBadMessage(std::string_view message);

  THREAD_CHECKER(thread_checker_);

 private:
  void OnDisconnect();

  const scoped_refptr<base::SequencedTaskRunner> receiver_task_runner_;
  mojo::ReceiverSet<mojom::URLLoaderFactory> receivers_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SELF_DELETING_URL_LOADER_FACTORY_H_
