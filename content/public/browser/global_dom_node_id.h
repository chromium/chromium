// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GLOBAL_DOM_NODE_ID_H_
#define CONTENT_PUBLIC_BROWSER_GLOBAL_DOM_NODE_ID_H_

#include "content/public/browser/weak_document_ptr.h"
#include "third_party/blink/public/common/dom/dom_node_id.h"

namespace content {

// Ties together a DOMNodeIdType to the Document from which it was generated.
// Code in the browser should prefer to pass around this object to avoid
// confusing which document the DOMNodeIdType comes from.
struct GlobalDOMNodeId {
  WeakDocumentPtr document;

  blink::DOMNodeIdType target_element_dom_id;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GLOBAL_DOM_NODE_ID_H_
