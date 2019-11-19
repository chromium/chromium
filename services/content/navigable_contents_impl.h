// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CONTENT_NAVIGABLE_CONTENTS_IMPL_H_
#define SERVICES_CONTENT_NAVIGABLE_CONTENTS_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/content/public/mojom/navigable_contents.mojom.h"
#include "services/content/public/mojom/navigable_contents_factory.mojom.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

class Service;
class NavigableContentsDelegate;
class NavigableContentsView;

// This is the state which backs an individual NavigableContents owned by some
// client of the Content Service. In terms of the classical Content API, this is
// roughly analogous to a WebContentsImpl.
class NavigableContentsImpl : public mojom::NavigableContents {
 public:
  NavigableContentsImpl(
      Service* service,
      mojom::NavigableContentsParamsPtr params,
      mojo::PendingReceiver<mojom::NavigableContents> receiver,
      mojo::PendingRemote<mojom::NavigableContentsClient> client);
  ~NavigableContentsImpl() override;

 private:
  // mojom::NavigableContents:
  void Navigate(const GURL& url, mojom::NavigateParamsPtr params) override;
  void GoBack(mojom::NavigableContents::GoBackCallback callback) override;
  void CreateView(CreateViewCallback callback) override;
  void Focus() override;
  void FocusThroughTabTraversal(bool reverse) override;

  // Used (indirectly) by the client library when run in the same process as the
  // service. See the |CreateView()| implementation for details.
  void EmbedInProcessClientView(NavigableContentsView* view);

  Service* const service_;

  mojo::Receiver<mojom::NavigableContents> receiver_;
  mojo::Remote<mojom::NavigableContentsClient> client_;
  std::unique_ptr<NavigableContentsDelegate> delegate_;
  gfx::NativeView native_content_view_;

  base::WeakPtrFactory<NavigableContentsImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NavigableContentsImpl);
};

}  // namespace content

#endif  // SERVICES_CONTENT_NAVIGABLE_CONTENTS_IMPL_H_
