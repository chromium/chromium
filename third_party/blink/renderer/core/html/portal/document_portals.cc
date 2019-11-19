// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/portal/document_portals.h"

#include "third_party/blink/renderer/core/html/portal/portal_contents.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

// static
const char DocumentPortals::kSupplementName[] = "DocumentPortals";

// static
DocumentPortals& DocumentPortals::From(Document& document) {
  DocumentPortals* supplement =
      Supplement<Document>::From<DocumentPortals>(document);
  if (!supplement) {
    supplement = MakeGarbageCollected<DocumentPortals>(document);
    Supplement<Document>::ProvideTo(document, supplement);
  }
  return *supplement;
}

DocumentPortals::DocumentPortals(Document& document)
    : Supplement<Document>(document) {}

void DocumentPortals::RegisterPortalContents(PortalContents* portal) {
  portals_.push_back(portal);
}

void DocumentPortals::DeregisterPortalContents(PortalContents* portal) {
  wtf_size_t index = portals_.Find(portal);
  if (index != WTF::kNotFound)
    portals_.EraseAt(index);
}

void DocumentPortals::Trace(Visitor* visitor) {
  Supplement<Document>::Trace(visitor);
  visitor->Trace(portals_);
  visitor->Trace(activating_portal_);
}

}  // namespace blink
