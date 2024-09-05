// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/page_navigator.h"

#include "content/public/browser/navigation_handle.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"

namespace content {

OpenURLParams::OpenURLParams(const GURL& url,
                             const Referrer& referrer,
                             WindowOpenDisposition disposition,
                             ui::PageTransition transition,
                             bool is_renderer_initiated)
    : url(url),
      referrer(referrer),
      disposition(disposition),
      transition(transition),
      is_renderer_initiated(is_renderer_initiated),
      user_gesture(!is_renderer_initiated) {}

OpenURLParams::OpenURLParams(const GURL& url,
                             const Referrer& referrer,
                             WindowOpenDisposition disposition,
                             ui::PageTransition transition,
                             bool is_renderer_initiated,
                             bool started_from_context_menu)
    : url(url),
      referrer(referrer),
      disposition(disposition),
      transition(transition),
      is_renderer_initiated(is_renderer_initiated),
      user_gesture(!is_renderer_initiated),
      started_from_context_menu(started_from_context_menu) {}

OpenURLParams::OpenURLParams(const GURL& url,
                             const Referrer& referrer,
                             FrameTreeNodeId frame_tree_node_id,
                             WindowOpenDisposition disposition,
                             ui::PageTransition transition,
                             bool is_renderer_initiated)
    : url(url),
      referrer(referrer),
      frame_tree_node_id(frame_tree_node_id),
      disposition(disposition),
      transition(transition),
      is_renderer_initiated(is_renderer_initiated),
      user_gesture(!is_renderer_initiated) {}

OpenURLParams::OpenURLParams(const OpenURLParams& other) = default;

OpenURLParams::~OpenURLParams() = default;

// static
OpenURLParams OpenURLParams::FromNavigationHandle(NavigationHandle* handle) {
  OpenURLParams params(
      handle->GetURL(), Referrer(handle->GetReferrer()),
      handle->GetFrameTreeNodeId(), WindowOpenDisposition::CURRENT_TAB,
      handle->GetPageTransition(), handle->IsRendererInitiated());

  params.initiator_origin = handle->GetInitiatorOrigin();
  params.initiator_base_url = handle->GetInitiatorBaseUrl();
  params.source_site_instance = handle->GetSourceSiteInstance();
  params.user_gesture = handle->HasUserGesture();
  params.started_from_context_menu = handle->WasStartedFromContextMenu();
  params.href_translate = handle->GetHrefTranslate();
  params.reload_type = handle->GetReloadType();

  // NavigationHandle will include all redirects that happened on the way to the
  // the current page in its redirect chain, including the current page itself
  // as the last entry. However OpenURLParams's redirect chain should only
  // include redirects that occurred before the current page. We need to remove
  // the last entry from `handle`'s redirect chain when initializing the
  // OpenURLParams.
  auto redirect_chain = handle->GetRedirectChain();
  DCHECK(redirect_chain.size());
  redirect_chain.pop_back();
  params.redirect_chain = std::move(redirect_chain);

  // TODO(lukasza): Consider also covering |post_data| (and |uses_post|) and
  // |extra_headers| (this is difficult, because we can't cast |handle| to
  // NavigationRequest*, because it may be MockNavigationHandle in unit tests).

#if DCHECK_IS_ON()
  DCHECK(params.Valid());
#endif
  return params;
}

#if DCHECK_IS_ON()
bool OpenURLParams::Valid() const {
  // Make sure URLs that result in an opaque origin have their initiator
  // information set so it can be used by downstream components. If |url|
  // is about:blank or data: URL and |initiator_origin| is set, then we also
  // need |source_site_instance| set so that
  // RenderFrameHostManager::DetermineSiteInstanceForURL() can select the
  // correct SiteInstance for these URLs.
  const bool is_data_or_about =
      url.IsAboutBlank() || url.SchemeIs(url::kDataScheme);
  const bool has_valid_initiator =
      initiator_origin.has_value() &&
      initiator_origin->GetTupleOrPrecursorTupleIfOpaque().IsValid();
  if (is_data_or_about && has_valid_initiator && !source_site_instance)
    return false;

  return true;
}
#endif  // DCHECK_IS_ON()

}  // namespace content
