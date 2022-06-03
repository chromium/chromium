// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_DOCUMENT_PORTALS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_DOCUMENT_PORTALS_H_

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class PortalContents;

// Tracks "active" portal contents associated with a document.
//
// An active contents is registered when it is created, either through creation
// of a new contents in the browser, or through adoption of an existing one.
// It is deregistered when that contents is closed, even though the
// PortalContents object may not yet have been garbage-collected.
//
// At most one such contents may be activating at a given time. If there is such
// a contents, it is also tracked by this object.
class DocumentPortals final : public GarbageCollected<DocumentPortals>,
                              public Supplement<Document> {
 public:
  static const char kSupplementName[];
  static DocumentPortals& From(Document&);

  void RegisterPortalContents(PortalContents*);
  void DeregisterPortalContents(PortalContents*);

  // Retrieves all portals in the document.
  const HeapVector<Member<PortalContents>>& GetPortals() const {
    return portals_;
  }

  // Returns true if any portal in the document is currently activating.
  bool IsPortalInDocumentActivating() const { return activating_portal_; }

  PortalContents* GetActivatingPortalContents() const {
    return activating_portal_;
  }
  void SetActivatingPortalContents(PortalContents* portal) {
    DCHECK(!activating_portal_);
    activating_portal_ = portal;
  }
  void ClearActivatingPortalContents() { activating_portal_ = nullptr; }

  explicit DocumentPortals(Document&);

  void Trace(Visitor*) const override;

 private:
  HeapVector<Member<PortalContents>> portals_;

  // Needed to keep the activating contents alive so that it can receive an IPC
  // reply from the browser process and respond accordingly (e.g. by resolving
  // the promise).
  //
  // At most one portal contents can be undergoing activation at a time.
  Member<PortalContents> activating_portal_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_DOCUMENT_PORTALS_H_
