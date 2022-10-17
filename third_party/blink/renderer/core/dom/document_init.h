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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_INIT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_INIT_H_

#include "base/dcheck_is_on.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class Agent;
class Document;
class ExecutionContext;
class LocalDOMWindow;
class LocalFrame;
class PluginData;

class CORE_EXPORT DocumentInit final {
  STACK_ALLOCATED();

 public:
  // Create a DocumentInit instance, then add a chain of calls to add optional
  // parameters to it.
  //
  // Example:
  //
  //   DocumentInit init = DocumentInit::Create()
  //       .WithExecutionContext(context)
  //       .WithURL(url);
  //
  // Before creating a Document from this DocumentInit, the caller must invoke
  // exactly one of:
  // * ForTest() - for unit-test-only cases
  // * WithWindow() - for navigations originating from DocumentLoader and
  //       attaching to a LocalDOMWindow.
  // * WithExecutionContext() - for all other cases
  //
  // Invoking init.CreateDocument() will construct a Document of the appropriate
  // subclass for the init's Type.
  // However, when the document type is known, it is acceptable to invoke the
  // constructor for Document (or the appropriate subclass) directly:
  //   Document* document = MakeGarbageCollected<Document>(init);
  static DocumentInit Create();

  DocumentInit(const DocumentInit&);
  ~DocumentInit();

  enum class Type {
    kHTML,
    kXHTML,
    kImage,
    kPlugin,
    kMedia,
    kSVG,
    kXML,
    kViewSource,
    kText,
    kUnspecified
  };

  DocumentInit& ForTest(ExecutionContext& execution_context);

  // Actually constructs the Document based on the provided state.
  Document* CreateDocument() const;

  bool IsSrcdocDocument() const;
  const KURL& FallbackSrcdocBaseURL() const;
  bool ShouldSetURL() const;

  DocumentInit& WithWindow(LocalDOMWindow*, Document* owner_document);
  LocalDOMWindow* GetWindow() const { return window_; }

  DocumentInit& WithToken(const DocumentToken& token);
  const DocumentToken& GetToken() const;

  DocumentInit& WithAgent(Agent& agent);
  Agent& GetAgent() const;

  DocumentInit& ForInitialEmptyDocument(bool empty);
  bool IsInitialEmptyDocument() const { return is_initial_empty_document_; }

  DocumentInit& ForPrerendering(bool is_prerendering);
  bool IsPrerendering() const { return is_prerendering_; }

  // Compute the type of document to be loaded inside a `frame`, given its
  // `mime_type`.
  //
  // In case of plugin handled by MimeHandlerview (which do not create a
  // PluginDocument), the type is Type::KHTML and `is_for_external_handler` is
  // set to true.
  static Type ComputeDocumentType(LocalFrame* frame,
                                  const String& mime_type,
                                  bool* is_for_external_handler = nullptr);
  DocumentInit& WithTypeFrom(const String& mime_type);
  Type GetType() const { return type_; }
  const String& GetMimeType() const { return mime_type_; }
  bool IsForExternalHandler() const { return is_for_external_handler_; }

  // Used when creating Documents not attached to a window.
  DocumentInit& WithExecutionContext(ExecutionContext*);
  ExecutionContext* GetExecutionContext() const { return execution_context_; }

  DocumentInit& WithURL(const KURL&);
  const KURL& Url() const { return url_; }

  const KURL& GetCookieUrl() const;

  DocumentInit& WithSrcdocDocument(bool is_srcdoc_document);
  DocumentInit& WithFallbackSrcdocBaseURL(const KURL& fallback_srcdoc_base_url);

  DocumentInit& WithWebBundleClaimedUrl(const KURL& web_bundle_claimed_url);
  const KURL& GetWebBundleClaimedUrl() const { return web_bundle_claimed_url_; }

  DocumentInit& WithUkmSourceId(ukm::SourceId ukm_source_id);
  ukm::SourceId UkmSourceId() const { return ukm_source_id_; }

 private:
  DocumentInit() = default;

  static PluginData* GetPluginData(LocalFrame* frame);

  Type type_ = Type::kUnspecified;
  bool is_prerendering_ = false;
  bool is_initial_empty_document_ = false;
  String mime_type_;
  LocalDOMWindow* window_ = nullptr;
  // Mutable because the token is lazily-generated on demand if no token is
  // explicitly set.
  mutable absl::optional<DocumentToken> token_;
  ExecutionContext* execution_context_ = nullptr;
  KURL url_;
  Document* owner_document_ = nullptr;
  Agent* agent_ = nullptr;

  // Whether we should treat the new document as "srcdoc" document. This
  // affects security checks, since srcdoc's content comes directly from
  // the parent document, not from loading a URL.
  bool is_srcdoc_document_ = false;
  KURL fallback_srcdoc_base_url_;

  // The claimed URL inside Web Bundle file from which the document is loaded.
  // This URL is used for window.location and document.URL and relative path
  // computation in the document.
  KURL web_bundle_claimed_url_;

  // Source id to set on the Document to be created.
  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;

  bool is_for_external_handler_ = false;

#if DCHECK_IS_ON()
  bool for_test_ = false;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_INIT_H_
