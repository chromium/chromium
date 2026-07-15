// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GLOBAL_DOM_NODE_ID_H_
#define CONTENT_PUBLIC_BROWSER_GLOBAL_DOM_NODE_ID_H_

#include "base/types/id_type.h"
#include "content/public/browser/weak_document_ptr.h"

namespace content {

// This is `DOMNodeId` used within Blink. Its value is only meaningful within
// the renderer that generated it, except 0 which is an invalid id.
using DOMNodeId = base::IdType32<class DOMNodeIdTag>;

// Ties together a DOMNodeId to the Document from which it was generated. Code
// in the browser should prefer to pass around this object to avoid confusing
// which document the DOMNodeId comes from.
struct GlobalDOMNodeId {
  WeakDocumentPtr document;

  DOMNodeId target_element_dom_id;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GLOBAL_DOM_NODE_ID_H_
