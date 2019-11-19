// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CONTENT_NAVIGABLE_CONTENTS_FACTORY_IMPL_H_
#define SERVICES_CONTENT_NAVIGABLE_CONTENTS_FACTORY_IMPL_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/content/public/mojom/navigable_contents_factory.mojom.h"

namespace content {

class Service;

// An implementation of the NavigableContentsFactory which backs every connected
// NavigableContentsFactory interface in a Content Service client. This creates
// instances of NavigableContentsImpl to fulfill |CreateContents()| requests.
//
// Instances of this class, and of the NavigableContentsImpls it creates, are
// all managed by the Service instance.
class NavigableContentsFactoryImpl : public mojom::NavigableContentsFactory {
 public:
  NavigableContentsFactoryImpl(
      Service* service,
      mojo::PendingReceiver<mojom::NavigableContentsFactory> receiver);
  ~NavigableContentsFactoryImpl() override;

 private:
  // mojom::NavigableContentsFactory:
  void CreateContents(
      mojom::NavigableContentsParamsPtr params,
      mojo::PendingReceiver<mojom::NavigableContents> receiver,
      mojo::PendingRemote<mojom::NavigableContentsClient> client) override;

  Service* const service_;
  mojo::Receiver<mojom::NavigableContentsFactory> receiver_;

  DISALLOW_COPY_AND_ASSIGN(NavigableContentsFactoryImpl);
};

}  // namespace content

#endif  // SERVICES_CONTENT_NAVIGABLE_CONTENTS_FACTORY_IMPL_H_
