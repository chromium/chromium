// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/portal/html_portal_element.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_unknown_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

HTMLPortalElement::HTMLPortalElement(Document& document)
    : HTMLFrameOwnerElement(HTMLNames::portalTag, document) {}

HTMLPortalElement::~HTMLPortalElement() {}

HTMLElement* HTMLPortalElement::Create(Document& document) {
  if (RuntimeEnabledFeatures::PortalsEnabled())
    return new HTMLPortalElement(document);
  return HTMLUnknownElement::Create(HTMLNames::portalTag, document);
}

void HTMLPortalElement::Navigate() {
  KURL url = GetNonEmptyURLAttribute(HTMLNames::srcAttr);
  if (!url.IsEmpty() && portal_ptr_) {
    portal_ptr_->Navigate(url);
  }
}

ScriptPromise HTMLPortalElement::activate(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (portal_ptr_) {
    portal_ptr_->Activate(WTF::Bind(
        [](HTMLPortalElement* portal, ScriptPromiseResolver* resolver,
           mojom::blink::PortalActivationStatus result) {
          switch (result) {
            case mojom::blink::PortalActivationStatus::kNotSupported:
              resolver->Reject(
                  DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                       "Portal activation is not supported."));
              break;

            case mojom::blink::PortalActivationStatus::kSuccess:
              resolver->Resolve();
              break;

            default:
              NOTREACHED();
          }
        },
        WrapPersistent(this), WrapPersistent(resolver)));
  } else {
    resolver->Reject(DOMException::Create(
        DOMExceptionCode::kInvalidStateError,
        "The HTMLPortalElement is not associated with a portal context."));
  }

  return promise;
}

HTMLPortalElement::InsertionNotificationRequest HTMLPortalElement::InsertedInto(
    ContainerNode& node) {
  auto result = HTMLFrameOwnerElement::InsertedInto(node);

  Document& document = GetDocument();

  if (node.IsInDocumentTree() && document.IsHTMLDocument()) {
    document.GetFrame()->GetInterfaceProvider().GetInterface(
        mojo::MakeRequest(&portal_ptr_));
    portal_ptr_->Init(WTF::Bind(
        [](HTMLPortalElement* portal,
           const base::UnguessableToken& portal_token) {
          portal->portal_token_ = portal_token;
        },
        WrapPersistent(this)));
    Navigate();
  }

  return result;
}

void HTMLPortalElement::RemovedFrom(ContainerNode& node) {
  HTMLFrameOwnerElement::RemovedFrom(node);

  Document& document = GetDocument();

  if (node.IsInDocumentTree() && document.IsHTMLDocument()) {
    portal_ptr_.reset();
  }
}

bool HTMLPortalElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == HTMLNames::srcAttr ||
         HTMLFrameOwnerElement::IsURLAttribute(attribute);
}

void HTMLPortalElement::ParseAttribute(
    const AttributeModificationParams& params) {
  HTMLFrameOwnerElement::ParseAttribute(params);

  if (params.name == HTMLNames::srcAttr)
    Navigate();
}

}  // namespace blink
