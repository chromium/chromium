// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_AUTOFILL_ASSISTANT_NODE_SIGNALS_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_AUTOFILL_ASSISTANT_NODE_SIGNALS_H_

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"

namespace blink {

struct BLINK_MODULES_EXPORT AutofillAssistantNodeFeatures {
  WebString html_tag;
  // TODO(sandromaggi): Expand for feature parity.
};

struct BLINK_MODULES_EXPORT AutofillAssistantNodeSignals {
  AutofillAssistantNodeFeatures node_features;

  // Stable ID for this element. This is the DOMNodeIds assigned to this node
  // which corresponds to the BackendNodeId in DevTools.
  int32_t backend_node_id;
};

BLINK_MODULES_EXPORT WebVector<AutofillAssistantNodeSignals>
GetAutofillAssistantNodeSignals(const WebDocument& web_document);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_AUTOFILL_ASSISTANT_NODE_SIGNALS_H_
