// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/content/navigable_contents_impl.h"

#include "base/bind.h"
#include "services/content/navigable_contents_delegate.h"
#include "services/content/public/cpp/buildflags.h"
#include "services/content/public/cpp/navigable_contents_view.h"
#include "services/content/service.h"
#include "services/content/service_delegate.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/controls/native/native_view_host.h"  // nogncheck

#if defined(USE_AURA)
#include "ui/aura/window.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)
#include "ui/base/ui_base_features.h"                       // nogncheck
#include "ui/views/mus/remote_view/remote_view_provider.h"  // nogncheck
#endif
#endif  // defined(TOOLKIT_VIEWS)

namespace content {

NavigableContentsImpl::NavigableContentsImpl(
    Service* service,
    mojom::NavigableContentsParamsPtr params,
    mojom::NavigableContentsRequest request,
    mojom::NavigableContentsClientPtr client)
    : service_(service),
      binding_(this, std::move(request)),
      client_(std::move(client)),
      delegate_(
          service_->delegate()->CreateNavigableContentsDelegate(*params,
                                                                client_.get())),
      native_content_view_(delegate_->GetNativeView()) {
  binding_.set_connection_error_handler(base::BindRepeating(
      &Service::RemoveNavigableContents, base::Unretained(service_), this));
}

NavigableContentsImpl::~NavigableContentsImpl() = default;

void NavigableContentsImpl::Navigate(const GURL& url,
                                     mojom::NavigateParamsPtr params) {
  // Ignore non-HTTP/HTTPS requests for now.
  if (!url.SchemeIsHTTPOrHTTPS())
    return;

  delegate_->Navigate(url, std::move(params));
}

void NavigableContentsImpl::CreateView(bool in_service_process,
                                       CreateViewCallback callback) {
  DCHECK(native_content_view_);

#if BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)
  if (!in_service_process) {
    remote_view_provider_ =
        std::make_unique<views::RemoteViewProvider>(native_content_view_);
    remote_view_provider_->GetEmbedToken(
        base::BindOnce(&NavigableContentsImpl::OnEmbedTokenReceived,
                       base::Unretained(this), std::move(callback)));
    return;
  }
#else
  if (!in_service_process) {
    DLOG(ERROR) << "Remote NavigableContentsView clients are not supported on "
                << "this platform.";
    return;
  }
#endif

  // Create and stash a new callback (indexed by token) which the in-process
  // client library can use to establish an "embedding" of the contents' view.
  auto token = base::UnguessableToken::Create();
  NavigableContentsView::RegisterInProcessEmbedCallback(
      token, base::BindOnce(&NavigableContentsImpl::EmbedInProcessClientView,
                            weak_ptr_factory_.GetWeakPtr()));
  std::move(callback).Run(token);
}

#if BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)
void NavigableContentsImpl::OnEmbedTokenReceived(
    CreateViewCallback callback,
    const base::UnguessableToken& token) {
#if defined(TOOLKIT_VIEWS) && defined(USE_AURA)
  DCHECK(native_content_view_);
  native_content_view_->Show();
#endif  // defined(TOOLKIT_VIEWS) && defined(USE_AURA)
  std::move(callback).Run(token);
}
#endif  // BUILDFLAG(ENABLE_REMOTE_NAVIGABLE_CONTENTS_VIEW)

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
