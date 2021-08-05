/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_DOCUMENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_DOCUMENT_H_

#include "net/cookies/site_for_cookies.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom-shared.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_draggable_region.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {

class Document;
class WebElement;
class WebFormElement;
class WebElementCollection;
class WebString;
class WebURL;
struct WebDistillabilityFeatures;

using WebStyleSheetKey = WebString;

// An enumeration used to enumerate usage of APIs that may prevent a document
// from entering the back forward cache. |kAllow| means usage of the API will
// not restrict the back forward cache. |kPossiblyDisallow| means usage of the
// API will be marked as such and the back forward cache may not allow the
// document to enter at its discretion.
enum class BackForwardCacheAware { kAllow, kPossiblyDisallow };

// Provides readonly access to some properties of a DOM document.
class WebDocument : public WebNode {
 public:
  enum CSSOrigin { kAuthorOrigin, kUserOrigin };

  WebDocument() = default;
  WebDocument(const WebDocument& e) = default;

  WebDocument& operator=(const WebDocument& e) {
    WebNode::Assign(e);
    return *this;
  }
  void Assign(const WebDocument& e) { WebNode::Assign(e); }

  BLINK_EXPORT WebURL Url() const;
  // Note: Security checks should use the getSecurityOrigin(), not url().
  BLINK_EXPORT WebSecurityOrigin GetSecurityOrigin() const;
  BLINK_EXPORT bool IsSecureContext() const;

  BLINK_EXPORT WebString Encoding() const;
  BLINK_EXPORT WebString ContentLanguage() const;
  BLINK_EXPORT WebString GetReferrer() const;
  BLINK_EXPORT absl::optional<SkColor> ThemeColor();
  // The url of the OpenSearch Description Document (if any).
  BLINK_EXPORT WebURL OpenSearchDescriptionURL() const;

  // Returns the frame the document belongs to or 0 if the document is
  // frameless.
  BLINK_EXPORT WebLocalFrame* GetFrame() const;
  BLINK_EXPORT bool IsHTMLDocument() const;
  BLINK_EXPORT bool IsXHTMLDocument() const;
  BLINK_EXPORT bool IsPluginDocument() const;
  BLINK_EXPORT WebURL BaseURL() const;
  BLINK_EXPORT ukm::SourceId GetUkmSourceId() const;

  // The SiteForCookies is used to compute whether this document
  // appears in a "third-party" context for the purpose of third-party
  // cookie blocking.
  BLINK_EXPORT net::SiteForCookies SiteForCookies() const;

  BLINK_EXPORT WebSecurityOrigin TopFrameOrigin() const;
  BLINK_EXPORT WebElement DocumentElement() const;
  BLINK_EXPORT WebElement Body() const;
  BLINK_EXPORT WebElement Head();
  BLINK_EXPORT WebString Title() const;
  BLINK_EXPORT WebString ContentAsTextForTesting() const;
  BLINK_EXPORT WebElementCollection All();
  BLINK_EXPORT WebVector<WebFormElement> Forms() const;
  BLINK_EXPORT WebURL CompleteURL(const WebString&) const;
  BLINK_EXPORT WebElement GetElementById(const WebString&) const;
  BLINK_EXPORT WebElement FocusedElement() const;

  // Inserts the given CSS source code as a style sheet in the document.
  BLINK_EXPORT WebStyleSheetKey
  InsertStyleSheet(const WebString& source_code,
                   const WebStyleSheetKey* = nullptr,
                   CSSOrigin = kAuthorOrigin,
                   BackForwardCacheAware = BackForwardCacheAware::kAllow);

  // Removes the CSS which was previously inserted by a call to
  // InsertStyleSheet().
  BLINK_EXPORT void RemoveInsertedStyleSheet(const WebStyleSheetKey&,
                                             CSSOrigin = kAuthorOrigin);

  // Arranges to call WebLocalFrameClient::didMatchCSS(frame(), ...) when one of
  // the selectors matches or stops matching an element in this document.
  // Each call to this method overrides any previous calls.
  BLINK_EXPORT void WatchCSSSelectors(const WebVector<WebString>& selectors);

  BLINK_EXPORT WebVector<WebDraggableRegion> DraggableRegions() const;

  BLINK_EXPORT WebURL CanonicalUrlForSharing() const;

  BLINK_EXPORT WebDistillabilityFeatures DistillabilityFeatures();

  BLINK_EXPORT void SetShowBeforeUnloadDialog(bool show_dialog);

  // See cc/paint/element_id.h for the definition of these id.
  BLINK_EXPORT uint64_t GetVisualViewportScrollingElementIdForTesting();

  BLINK_EXPORT bool IsLoaded();

  // Returns true if the document is in prerendering.
  BLINK_EXPORT bool IsPrerendering();

  // Return true if  accessibility processing has been enabled.
  BLINK_EXPORT bool IsAccessibilityEnabled();

  // Adds `callback` to the post-prerendering activation steps.
  // https://jeremyroman.github.io/alternate-loading-modes/#document-post-prerendering-activation-steps-list
  BLINK_EXPORT void AddPostPrerenderingActivationStep(
      base::OnceClosure callback);

  // Sets a cookie manager which can be used for this document.
  BLINK_EXPORT void SetCookieManager(
      CrossVariantMojoRemote<
          network::mojom::RestrictedCookieManagerInterfaceBase> cookie_manager);

#if INSIDE_BLINK
  BLINK_EXPORT WebDocument(Document*);
  BLINK_EXPORT WebDocument& operator=(Document*);
  BLINK_EXPORT operator Document*() const;
#endif
};

DECLARE_WEB_NODE_TYPE_CASTS(WebDocument);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_DOCUMENT_H_
