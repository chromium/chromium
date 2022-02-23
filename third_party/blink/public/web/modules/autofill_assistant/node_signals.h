// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_AUTOFILL_ASSISTANT_NODE_SIGNALS_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_AUTOFILL_ASSISTANT_NODE_SIGNALS_H_

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"

namespace blink {

// Features extracted directly from the DOM node of interest.
struct BLINK_MODULES_EXPORT AutofillAssistantNodeFeatures {
  // Inner text and other text that is attached to the node and available to
  // the user, e.g. input placeholders or select options.
  WebVector<WebString> text;
  // Aria label, descriptions and placeholders - joined with ' '. Representation
  // of this dom element to users with disabilities. This is a strong signal
  // that this dom element is relevant to the user.
  WebString aria;
  // Tag of the dom element.
  WebString html_tag;
  // Value of the "type" attribute. Only populated for <input> elements.
  WebString type;
  // Text from the computed attributes "id", "name", "title", "content" and
  // "label" - joined with ' '.
  WebString invisible_attributes;
};

// Features extracted from labels that are directly related to the DOM node of
// interest.
struct BLINK_MODULES_EXPORT AutofillAssistantLabelFeatures {
  // Text of the label.
  WebVector<WebString> text;
};

// Features extracted from DOM nodes that are loosely related to the DOM node
// of interest, e.g. headers.
struct BLINK_MODULES_EXPORT AutofillAssistantContextFeatures {
  // Text of headers.
  WebVector<WebString> header_text;
};

struct BLINK_MODULES_EXPORT AutofillAssistantNodeSignals {
  AutofillAssistantNodeFeatures node_features;
  AutofillAssistantLabelFeatures label_features;
  AutofillAssistantContextFeatures context_features;

  // Stable ID for this element. This is the DOMNodeIds assigned to this node
  // which corresponds to the BackendNodeId in DevTools.
  int32_t backend_node_id;
};

BLINK_MODULES_EXPORT WebVector<AutofillAssistantNodeSignals>
GetAutofillAssistantNodeSignals(const WebDocument& web_document);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_AUTOFILL_ASSISTANT_NODE_SIGNALS_H_
