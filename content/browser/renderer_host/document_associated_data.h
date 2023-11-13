// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DOCUMENT_ASSOCIATED_DATA_H_
#define CONTENT_BROWSER_RENDERER_HOST_DOCUMENT_ASSOCIATED_DATA_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/types/pass_key.h"
#include "base/unguessable_token.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace content {

namespace internal {
class DocumentServiceBase;
}

class NavigationOrDocumentHandle;
class PageImpl;
class RenderFrameHostImpl;

// Container for arbitrary document-associated feature-specific data. Should
// be reset when committing a cross-document navigation in this
// RenderFrameHost. RenderFrameHostImpl stores internal members here
// directly while consumers of RenderFrameHostImpl should store data via
// GetDocumentUserData(). Please refer to the description at
// content/public/browser/document_user_data.h for more details.
class DocumentAssociatedData : public base::SupportsUserData {
 public:
  // Helper for looking up a RenderFrameHostImpl based on the DocumentToken.
  // Restricted to RenderFrameHostImpl, which performs additional security
  // checks.
  static RenderFrameHostImpl* GetDocumentFromToken(
      base::PassKey<RenderFrameHostImpl>,
      const blink::DocumentToken& token);

  explicit DocumentAssociatedData(RenderFrameHostImpl& document,
                                  const blink::DocumentToken& token);
  ~DocumentAssociatedData() override;
  DocumentAssociatedData(const DocumentAssociatedData&) = delete;
  DocumentAssociatedData& operator=(const DocumentAssociatedData&) = delete;

  // Removes all services. We do that on the destructor of
  // DocumentAssociatedData, but also before resetting
  // `document_associated_data_` in RenderFrameHostImpl. This is because
  // otherwise, when services in DocumentAssociatedData try to remove
  // themselves from DocumentAssociatedData through the RenderFrameHostImpl
  // optional, it will not be valid anymore.
  void RemoveAllServices();

  // An opaque token that uniquely identifies the document currently
  // associated with this RenderFrameHost. Note that in the case of
  // speculative RenderFrameHost that has not yet committed, the renderer side
  // will not have a document with a matching token!
  const blink::DocumentToken& token() const { return token_; }

  // The Page object associated with the main document. It is nullptr for
  // subframes.
  PageImpl* owned_page() { return owned_page_.get(); }

  const PageImpl* owned_page() const { return owned_page_.get(); }

  // Indicates whether `blink::mojom::DidDispatchDOMContentLoadedEvent` was
  // called for this document or not.
  bool dom_content_loaded() const { return dom_content_loaded_; }
  void MarkDomContentLoaded() { dom_content_loaded_ = true; }

  // Prerender2:
  //
  // The URL that `blink.mojom.LocalFrameHost::DidFinishLoad()` passed to
  // DidFinishLoad, nullopt if DidFinishLoad wasn't called for this document
  // or this document is not in prerendering. This is used to defer and
  // dispatch DidFinishLoad notification on prerender activation.
  const std::optional<GURL>& pending_did_finish_load_url_for_prerendering()
      const {
    return pending_did_finish_load_url_for_prerendering_;
  }
  void set_pending_did_finish_load_url_for_prerendering(const GURL& url) {
    pending_did_finish_load_url_for_prerendering_.emplace(url);
  }
  void reset_pending_did_finish_load_url_for_prerendering() {
    pending_did_finish_load_url_for_prerendering_.reset();
  }

  // Reporting API:
  //
  // Contains the reporting source token for this document, which will be
  // associated with the reporting endpoint configuration in the network
  // service, as well as with any reports which are queued by this document.
  const base::UnguessableToken& reporting_source() const {
    return token_.value();
  }

  // "Owned" but not with std::unique_ptr, as a DocumentServiceBase is
  // allowed to delete itself directly.
  std::vector<internal::DocumentServiceBase*>& services() { return services_; }

  // This handle supports a seamless transfer from a navigation to a committed
  // document.
  const scoped_refptr<NavigationOrDocumentHandle>&
  navigation_or_document_handle() const {
    return navigation_or_document_handle_;
  }
  void set_navigation_or_document_handle(
      scoped_refptr<NavigationOrDocumentHandle> handle);

  // See comments for |RenderFrameHostImpl::GetDevToolsNavigationToken()| for
  // more details.
  const absl::optional<base::UnguessableToken>& devtools_navigation_token()
      const {
    return devtools_navigation_token_;
  }

  void set_devtools_navigation_token(
      const base::UnguessableToken& devtools_navigation_token) {
    devtools_navigation_token_ = devtools_navigation_token;
  }

  // Produces weak pointers to the hosting RenderFrameHostImpl. This is
  // invalidated whenever DocumentAssociatedData is destroyed, due to
  // RenderFrameHost deletion or cross-document navigation.
  base::WeakPtrFactory<RenderFrameHostImpl>& weak_factory() {
    return weak_factory_;
  }

 private:
  const blink::DocumentToken token_;
  std::unique_ptr<PageImpl> owned_page_;
  bool dom_content_loaded_ = false;
  std::optional<GURL> pending_did_finish_load_url_for_prerendering_;
  std::vector<internal::DocumentServiceBase*> services_;
  scoped_refptr<NavigationOrDocumentHandle> navigation_or_document_handle_;
  std::optional<base::UnguessableToken> devtools_navigation_token_;

  base::WeakPtrFactory<RenderFrameHostImpl> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DOCUMENT_ASSOCIATED_DATA_H_
