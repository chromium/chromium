// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/content/navigable_contents_impl.h"

#include "base/bind.h"
#include "services/content/navigable_contents_delegate.h"
#include "services/content/public/cpp/navigable_contents_view.h"
#include "services/content/service.h"
#include "services/content/service_delegate.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"  // nogncheck
#endif

namespace content {

NavigableContentsImpl::NavigableContentsImpl(
    Service* service,
    mojom::NavigableContentsParamsPtr params,
    mojo::PendingReceiver<mojom::NavigableContents> receiver,
    mojo::PendingRemote<mojom::NavigableContentsClient> client)
    : service_(service),
      receiver_(this, std::move(receiver)),
      client_(std::move(client)),
      delegate_(
          service_->delegate()->CreateNavigableContentsDelegate(*params,
                                                                client_.get())),
      native_content_view_(delegate_->GetNativeView()) {
  receiver_.set_disconnect_handler(base::BindRepeating(
      &Service::RemoveNavigableContents, base::Unretained(service_), this));
}

NavigableContentsImpl::~NavigableContentsImpl() = default;

void NavigableContentsImpl::Navigate(const GURL& url,
                                     mojom::NavigateParamsPtr params) {
  // Ignore non-HTTP/HTTPS/data requests for now.
  if (!url.SchemeIsHTTPOrHTTPS() && !url.SchemeIs(url::kDataScheme))
    return;

  delegate_->Navigate(url, std::move(params));
}

void NavigableContentsImpl::GoBack(
    mojom::NavigableContents::GoBackCallback callback) {
  delegate_->GoBack(std::move(callback));
}

void NavigableContentsImpl::CreateView(CreateViewCallback callback) {
  // Create and stash a new callback (indexed by token) which the in-process
  // client library can use to establish an "embedding" of the contents' view.
  auto token = base::UnguessableToken::Create();
  NavigableContentsView::RegisterInProcessEmbedCallback(
      token, base::BindOnce(&NavigableContentsImpl::EmbedInProcessClientView,
                            weak_ptr_factory_.GetWeakPtr()));
  std::move(callback).Run(token);
}

void NavigableContentsImpl::Focus() {
  delegate_->Focus();
}

void NavigableContentsImpl::FocusThroughTabTraversal(bool reverse) {
  delegate_->FocusThroughTabTraversal(reverse);
}

void NavigableContentsImpl::EmbedInProcessClientView(
    NavigableContentsView* view) {
  DCHECK(native_content_view_);
#if defined(TOOLKIT_VIEWS) && defined(USE_AURA)
  view->native_view()->AddChild(native_content_view_);
  native_content_view_->Show();
#else
  // TODO(https://crbug.com/855092): Support embedding of other native client
  // views without Views + Aura.
  NOTREACHED()
      << "NavigableContents views are currently only supported on Views UI.";
#endif
}

}  // namespace content
