// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CLIENT_HINTS_CONTROLLER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_CLIENT_HINTS_CONTROLLER_DELEGATE_H_

#include <memory>

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"
#include "ui/gfx/geometry/size_f.h"
#include "url/origin.h"

class GURL;

namespace blink {
class EnabledClientHints;
struct UserAgentMetadata;
}  // namespace blink

namespace network {
class NetworkQualityTracker;
}  // namespace network

namespace content {

class RenderFrameHost;

class CONTENT_EXPORT ClientHintsControllerDelegate {
 public:
  virtual ~ClientHintsControllerDelegate() = default;

  virtual network::NetworkQualityTracker* GetNetworkQualityTracker() = 0;

  // Get which client hints opt-ins were persisted on current origin.
  virtual void GetAllowedClientHintsFromSource(
      const url::Origin& origin,
      blink::EnabledClientHints* client_hints) = 0;

  // Checks is script is enabled. |parent_rfh| is the document embedding the
  // frame in which the request is taking place (it is null for outermost main
  // frame requests).
  virtual bool IsJavaScriptAllowed(const GURL& url,
                                   RenderFrameHost* parent_rfh) = 0;

  // Returns true iff cookies are blocked for the URL/RFH or third-party cookies
  // are disabled in the user agent.
  virtual bool AreThirdPartyCookiesBlocked(const GURL& url,
                                           RenderFrameHost* rfh) = 0;

  virtual blink::UserAgentMetadata GetUserAgentMetadata() = 0;

  virtual void PersistClientHints(
      const url::Origin& primary_origin,
      RenderFrameHost* parent_rfh,
      const std::vector<network::mojom::WebClientHintsType>& client_hints) = 0;

  // Optionally implemented by implementations used in tests. Clears all hints
  // that would have been returned by GetAllowedClientHintsFromSource(),
  // regardless of whether they were added via PersistClientHints() or
  // SetAdditionalHints().
  virtual void ResetForTesting() {}

  // Sets additional `hints` that this delegate should add to the
  // blink::EnabledClientHints object affected by
  // |GetAllowedClientHintsFromSource|. This is for when there are additional
  // client hints to be added to a request that are not in storage.
  virtual void SetAdditionalClientHints(
      const std::vector<network::mojom::WebClientHintsType>&) = 0;

  // Clears the additional hints set by |SetAdditionalHints|.
  virtual void ClearAdditionalClientHints() = 0;

  // Used to track the visible viewport size. This value is only used when the
  // viewport size cannot be directly obtained, such as for prefetch requests
  // and for tab restores. The embedder is responsible for calling this when the
  // size of the visible viewport changes.
  virtual void SetMostRecentMainFrameViewportSize(
      const gfx::Size& viewport_size) = 0;
  virtual gfx::Size GetMostRecentMainFrameViewportSize() = 0;

  // Optionally implemented for use in tests. Forces gfx::Size to be empty in
  // all future navigations.
  virtual void ForceEmptyViewportSizeForTesting(
      bool should_force_empty_viewport_size) {}
  virtual bool ShouldForceEmptyViewportSize();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CLIENT_HINTS_CONTROLLER_DELEGATE_H_
