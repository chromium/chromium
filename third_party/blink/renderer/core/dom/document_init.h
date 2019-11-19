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

#include "services/network/public/mojom/ip_address_space.mojom-shared.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/platform/web_insecure_request_policy.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/sandbox_flags.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_registration_context.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class ContentSecurityPolicy;
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
  //   Document* document = MakeGarbageCollected<Document>(init);
  static DocumentInit Create();
  static DocumentInit CreateWithImportsController(HTMLImportsController*);

  DocumentInit(const DocumentInit&);
  ~DocumentInit();

  HTMLImportsController* ImportsController() const {
    return imports_controller_;
  }

  bool HasSecurityContext() const { return MasterDocumentLoader(); }
  bool IsSrcdocDocument() const;
  bool ShouldSetURL() const;
  WebSandboxFlags GetSandboxFlags() const;
  WebInsecureRequestPolicy GetInsecureRequestPolicy() const;
  const SecurityContext::InsecureNavigationsSet* InsecureNavigationsToUpgrade()
      const;
  bool GrantLoadLocalResources() const { return grant_load_local_resources_; }

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

  scoped_refptr<SecurityOrigin> GetDocumentOrigin() const;

  // Specifies the Document to inherit security configurations from.
  DocumentInit& WithOwnerDocument(Document*);
  Document* OwnerDocument() const { return owner_document_.Get(); }

  // Specifies the SecurityOrigin in which the URL was requested. This is
  // relevant for determining properties of the resulting document's origin
  // when loading data: and about: schemes.
  DocumentInit& WithInitiatorOrigin(
      scoped_refptr<const SecurityOrigin> initiator_origin);

  DocumentInit& WithOriginToCommit(
      scoped_refptr<SecurityOrigin> origin_to_commit);
  const scoped_refptr<SecurityOrigin>& OriginToCommit() const {
    return origin_to_commit_;
  }

  DocumentInit& WithIPAddressSpace(
      network::mojom::IPAddressSpace ip_address_space);
  network::mojom::IPAddressSpace GetIPAddressSpace() const;

  DocumentInit& WithSrcdocDocument(bool is_srcdoc_document);
  DocumentInit& WithBlockedByCSP(bool blocked_by_csp);
  DocumentInit& WithGrantLoadLocalResources(bool grant_load_local_resources);

  DocumentInit& WithRegistrationContext(V0CustomElementRegistrationContext*);
  V0CustomElementRegistrationContext* RegistrationContext(Document*) const;
  DocumentInit& WithNewRegistrationContext();

  DocumentInit& WithFeaturePolicyHeader(const String& header);
  const String& FeaturePolicyHeader() const { return feature_policy_header_; }

  DocumentInit& WithOriginTrialsHeader(const String& header);
  const String& OriginTrialsHeader() const { return origin_trials_header_; }

  DocumentInit& WithSandboxFlags(WebSandboxFlags flags);

  DocumentInit& WithContentSecurityPolicy(ContentSecurityPolicy* policy);
  DocumentInit& WithContentSecurityPolicyFromContextDoc();
  ContentSecurityPolicy* GetContentSecurityPolicy() const;

  DocumentInit& WithFramePolicy(
      const base::Optional<FramePolicy>& frame_policy);
  const base::Optional<FramePolicy>& GetFramePolicy() const {
    return frame_policy_;
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

  // Initiator origin is used for calculating the document origin when the
  // navigation is started in a different process. In such cases, the document
  // which initiates the navigation sends its origin to the browser process and
  // it is provided by the browser process here. It is used for cases such as
  // data: URLs, which inherit their origin from the initiator of the
  // navigation.
  // Note: about:blank should also behave this way, however currently it
  // inherits its origin from the parent frame or opener, regardless of whether
  // it is the initiator or not.
  scoped_refptr<const SecurityOrigin> initiator_origin_;

  // The |origin_to_commit_| is to be used directly without calculating the
  // document origin at initialization time. It is specified by the browser
  // process for session history navigations. This allows us to preserve
  // the origin across session history and ensure the exact same origin
  // is present on such navigations to URLs that inherit their origins (e.g.
  // about:blank and data: URLs).
  scoped_refptr<SecurityOrigin> origin_to_commit_;

  // Whether we should treat the new document as "srcdoc" document. This
  // affects security checks, since srcdoc's content comes directly from
  // the parent document, not from loading a URL.
  bool is_srcdoc_document_ = false;

  // Whether the actual document was blocked by csp and we are creating a dummy
  // empty document instead.
  bool blocked_by_csp_ = false;

  // Whether the document should be able to access local file:// resources.
  bool grant_load_local_resources_ = false;

  Member<V0CustomElementRegistrationContext> registration_context_;
  bool create_new_registration_context_;

  // The feature policy set via response header.
  String feature_policy_header_;

  // The origin trial set via response header.
  String origin_trials_header_;

  // Additional sandbox flags
  WebSandboxFlags sandbox_flags_ = WebSandboxFlags::kNone;

  // Loader's CSP
  Member<ContentSecurityPolicy> content_security_policy_;
  bool content_security_policy_from_context_doc_;

  network::mojom::IPAddressSpace ip_address_space_ =
      network::mojom::IPAddressSpace::kUnknown;

  // The frame policy snapshot from the beginning of navigation.
  base::Optional<FramePolicy> frame_policy_ = base::nullopt;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_INIT_H_
