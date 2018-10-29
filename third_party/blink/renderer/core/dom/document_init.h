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

#include "third_party/blink/public/platform/web_insecure_request_policy.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/sandbox_flags.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_registration_context.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class Document;
class DocumentLoader;
class LocalFrame;
class HTMLImportsController;
class Settings;

class CORE_EXPORT DocumentInit final {
  STACK_ALLOCATED();

 public:
  // Use either of the following methods to create a DocumentInit instance, and
  // then add a chain of calls to the .WithFooBar() methods to add optional
  // parameters to it.
  //
  // Example:
  //
  //   DocumentInit init = DocumentInit::Create()
  //       .WithDocumentLoader(loader)
  //       .WithContextDocument(context_document)
  //       .WithURL(url);
  //   Document* document = Document::Create(init);
  static DocumentInit Create();
  static DocumentInit CreateWithImportsController(HTMLImportsController*);

  DocumentInit(const DocumentInit&);
  ~DocumentInit();

  HTMLImportsController* ImportsController() const {
    return imports_controller_;
  }

  bool HasSecurityContext() const { return MasterDocumentLoader(); }
  bool ShouldTreatURLAsSrcdocDocument() const;
  bool ShouldSetURL() const;
  SandboxFlags GetSandboxFlags() const;
  bool IsHostedInReservedIPRange() const;
  WebInsecureRequestPolicy GetInsecureRequestPolicy() const;
  SecurityContext::InsecureNavigationsSet* InsecureNavigationsToUpgrade() const;

  Settings* GetSettings() const;

  DocumentInit& WithDocumentLoader(DocumentLoader*);
  LocalFrame* GetFrame() const;

  // Used by the DOMImplementation and DOMParser to pass their parent Document
  // so that the created Document will return the Document when the
  // ContextDocument() method is called.
  DocumentInit& WithContextDocument(Document*);
  Document* ContextDocument() const;

  DocumentInit& WithURL(const KURL&);
  const KURL& Url() const { return url_; }

  // Specifies the Document to inherit security configurations from.
  DocumentInit& WithOwnerDocument(Document*);
  Document* OwnerDocument() const { return owner_document_.Get(); }

  DocumentInit& WithRegistrationContext(V0CustomElementRegistrationContext*);
  V0CustomElementRegistrationContext* RegistrationContext(Document*) const;
  DocumentInit& WithNewRegistrationContext();

  DocumentInit& WithPreviousDocumentCSP(const ContentSecurityPolicy*);
  const ContentSecurityPolicy* PreviousDocumentCSP() const {
    return previous_csp_.Get();
  }

 private:
  DocumentInit(HTMLImportsController*);

  // For a Document associated directly with a frame, this will be the
  // DocumentLoader driving the commit. For an import, XSLT-generated
  // document, etc., it will be the DocumentLoader that drove the commit
  // of its owning Document.
  DocumentLoader* MasterDocumentLoader() const;

  Member<DocumentLoader> document_loader_;
  Member<Document> parent_document_;

  Member<HTMLImportsController> imports_controller_;

  Member<Document> context_document_;
  KURL url_;
  Member<Document> owner_document_;

  Member<V0CustomElementRegistrationContext> registration_context_;
  bool create_new_registration_context_;

  Member<const ContentSecurityPolicy> previous_csp_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_INIT_H_
