/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/dom/document_init.h"

#include "base/uuid.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/core/dom/sink_document.h"
#include "third_party/blink/renderer/core/dom/xml_document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_view_source_document.h"
#include "third_party/blink/renderer/core/html/image_document.h"
#include "third_party/blink/renderer/core/html/json_document.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/media_document.h"
#include "third_party/blink/renderer/core/html/plugin_document.h"
#include "third_party/blink/renderer/core/html/text_document.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/plugin_data.h"
#include "third_party/blink/renderer/platform/network/mime/content_type.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// static
DocumentInit DocumentInit::Create() {
  return DocumentInit();
}

DocumentInit::DocumentInit(const DocumentInit&) = default;

DocumentInit::~DocumentInit() = default;

DocumentInit& DocumentInit::ForTest(ExecutionContext& execution_context) {
  DCHECK(!execution_context_);
  DCHECK(!window_);
  DCHECK(!agent_);
#if DCHECK_IS_ON()
  DCHECK(!for_test_);
  for_test_ = true;
#endif
  execution_context_ = &execution_context;
  agent_ = execution_context.GetAgent();
  return *this;
}

bool DocumentInit::ShouldSetURL() const {
  return (window_ && !window_->GetFrame()->IsMainFrame()) || !url_.IsEmpty();
}

bool DocumentInit::IsSrcdocDocument() const {
  return window_ && !window_->GetFrame()->IsMainFrame() && is_srcdoc_document_;
}

bool DocumentInit::IsAboutBlankDocument() const {
  return window_ && url_.IsAboutBlankURL();
}

const KURL& DocumentInit::FallbackBaseURL() const {
  DCHECK(IsSrcdocDocument() || IsAboutBlankDocument() ||
         IsInitialEmptyDocument() || is_for_javascript_url_ ||
         is_for_discard_ || fallback_base_url_.IsEmpty())
      << " url = " << url_ << ", fallback_base_url = " << fallback_base_url_;
  return fallback_base_url_;
}

DocumentInit& DocumentInit::WithWindow(LocalDOMWindow* window,
                                       Document* owner_document) {
  DCHECK(!window_);
  DCHECK(!execution_context_);
  DCHECK(!agent_);
#if DCHECK_IS_ON()
  DCHECK(!for_test_);
#endif
  DCHECK(window);
  window_ = window;
  execution_context_ = window;
  agent_ = window->GetAgent();
  owner_document_ = owner_document;
  return *this;
}

DocumentInit& DocumentInit::WithAgent(Agent& agent) {
  DCHECK(!agent_);
#if DCHECK_IS_ON()
  DCHECK(!for_test_);
#endif
  agent_ = &agent;
  return *this;
}

Agent& DocumentInit::GetAgent() const {
  DCHECK(agent_);
  return *agent_;
}

DocumentInit& DocumentInit::WithToken(const DocumentToken& token) {
  token_ = token;
  return *this;
}

const std::optional<DocumentToken>& DocumentInit::GetToken() const {
  return token_;
}

DocumentInit& DocumentInit::ForInitialEmptyDocument(bool empty) {
  is_initial_empty_document_ = empty;
  return *this;
}

DocumentInit& DocumentInit::ForPrerendering(bool is_prerendering) {
  is_prerendering_ = is_prerendering;
  return *this;
}

// static
DocumentInit::Type DocumentInit::ComputeDocumentType(
    LocalFrame* frame,
    const String& mime_type,
    bool* is_for_external_handler) {
  if (frame && frame->InViewSourceMode())
    return Type::kViewSource;

  // Plugins cannot take HTML and XHTML from us, and we don't even need to
  // initialize the plugin database for those.
  if (mime_type == "text/html")
    return Type::kHTML;

  if (mime_type == "application/xhtml+xml")
    return Type::kXHTML;

  // multipart/x-mixed-replace is only supported for images.
  if (MIMETypeRegistry::IsSupportedImageResourceMIMEType(mime_type) ||
      mime_type == "multipart/x-mixed-replace") {
    return Type::kImage;
  }

  if (HTMLMediaElement::GetSupportsType(ContentType(mime_type)))
    return Type::kMedia;

  if (frame && frame->GetPage() && frame->Loader().AllowPlugins()) {
    PluginData* plugin_data = GetPluginData(frame);

    // Everything else except text/plain can be overridden by plugins.
    // Disallowing plugins to use text/plain prevents plugins from hijacking a
    // fundamental type that the browser is expected to handle, and also serves
    // as an optimization to prevent loading the plugin database in the common
    // case.
    if (mime_type != "text/plain" && plugin_data &&
        plugin_data->SupportsMimeType(mime_type)) {
      // Plugins handled by MimeHandlerView do not create a PluginDocument. They
      // are rendered inside cross-process frames and the notion of a PluginView
      // (which is associated with PluginDocument) is irrelevant here.
      if (plugin_data->IsExternalPluginMimeType(mime_type)) {
        if (is_for_external_handler)
          *is_for_external_handler = true;
        return Type::kHTML;
      }

      return Type::kPlugin;
    }
  }

  if (MIMETypeRegistry::IsSupportedJavaScriptMIMEType(mime_type) ||
      MIMETypeRegistry::IsJSONMimeType(mime_type) ||
      MIMETypeRegistry::IsPlainTextMIMEType(mime_type)) {
    return Type::kText;
  }

  if (mime_type == "image/svg+xml")
    return Type::kSVG;

  if (MIMETypeRegistry::IsXMLMIMEType(mime_type))
    return Type::kXML;

  return Type::kHTML;
}

// static
PluginData* DocumentInit::GetPluginData(LocalFrame* frame) {
  return frame->GetPage()->GetPluginData();
}

DocumentInit& DocumentInit::WithTypeFrom(const String& mime_type) {
  mime_type_ = mime_type;
  type_ = ComputeDocumentType(window_ ? window_->GetFrame() : nullptr,
                              mime_type_, &is_for_external_handler_);
  return *this;
}

DocumentInit& DocumentInit::WithExecutionContext(
    ExecutionContext* execution_context) {
  DCHECK(!execution_context_);
  DCHECK(!window_);
  DCHECK(!agent_);
#if DCHECK_IS_ON()
  DCHECK(!for_test_);
#endif
  execution_context_ = execution_context;
  return *this;
}

DocumentInit& DocumentInit::WithURL(const KURL& url) {
  DCHECK(url_.IsNull());
  url_ = url;
  return *this;
}

const KURL& DocumentInit::GetCookieUrl() const {
  const KURL& cookie_url =
      owner_document_ ? owner_document_->CookieURL() : url_;

  // An "about:blank" should inherit the `cookie_url` from the initiator of the
  // navigation, but sometimes "about:blank" may commit without an
  // `owner_document` (e.g. if the original initiator has been navigated away).
  // In such scenario, it is important to use a safe `cookie_url` (e.g.
  // kCookieAverseUrl) to avoid triggering mojo::ReportBadMessage and renderer
  // kills via RestrictedCookieManager::ValidateAccessToCookiesAt.
  //
  // TODO(https://crbug.com/1176291): Correctly inherit the `cookie_url` from
  // the initiator.
  if (cookie_url.IsAboutBlankURL()) {
    // Signify a cookie-averse document [1] with an null URL.  See how
    // CookiesJar::GetCookies and other methods check `cookie_url` against
    // KURL::IsEmpty.
    //
    // [1] https://html.spec.whatwg.org/#cookie-averse-document-object
    const KURL& kCookieAverseUrl = NullURL();

    return kCookieAverseUrl;
  }

  return cookie_url;
}

DocumentInit& DocumentInit::WithSrcdocDocument(bool is_srcdoc_document) {
  is_srcdoc_document_ = is_srcdoc_document;
  return *this;
}

DocumentInit& DocumentInit::WithFallbackBaseURL(const KURL& fallback_base_url) {
  fallback_base_url_ = fallback_base_url;
  return *this;
}

DocumentInit& DocumentInit::WithJavascriptURL(bool is_for_javascript_url) {
  is_for_javascript_url_ = is_for_javascript_url;
  return *this;
}

DocumentInit& DocumentInit::ForDiscard(bool is_for_discard) {
  is_for_discard_ = is_for_discard;
  return *this;
}

DocumentInit& DocumentInit::WithUkmSourceId(ukm::SourceId ukm_source_id) {
  ukm_source_id_ = ukm_source_id;
  return *this;
}

DocumentInit& DocumentInit::WithBaseAuctionNonce(
    base::Uuid base_auction_nonce) {
  base_auction_nonce_ = base_auction_nonce;
  return *this;
}

Document* DocumentInit::CreateDocument() const {
#if DCHECK_IS_ON()
  DCHECK(execution_context_);
  DCHECK(agent_);
#endif
  switch (type_) {
    case Type::kHTML:
      return MakeGarbageCollected<HTMLDocument>(*this);
    case Type::kXHTML:
      return XMLDocument::CreateXHTML(*this);
    case Type::kImage:
      return MakeGarbageCollected<ImageDocument>(*this);
    case Type::kPlugin: {
      DCHECK(window_);
      if (window_->IsSandboxed(
              network::mojom::blink::WebSandboxFlags::kPlugins)) {
        return MakeGarbageCollected<SinkDocument>(*this);
      }
      return MakeGarbageCollected<PluginDocument>(*this);
    }
    case Type::kMedia:
      return MakeGarbageCollected<MediaDocument>(*this);
    case Type::kSVG:
      return XMLDocument::CreateSVG(*this);
    case Type::kXML:
      return MakeGarbageCollected<XMLDocument>(*this);
    case Type::kViewSource:
      return MakeGarbageCollected<HTMLViewSourceDocument>(*this);
    case Type::kText: {
      if (MIMETypeRegistry::IsJSONMimeType(mime_type_)) {
        return MakeGarbageCollected<JSONDocument>(*this);
      }
      return MakeGarbageCollected<TextDocument>(*this);
    }
    case Type::kUnspecified:
      [[fallthrough]];
    default:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

}  // namespace blink
