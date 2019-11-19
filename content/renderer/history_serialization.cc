// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/history_serialization.h"

#include <stddef.h>
#include <algorithm>

#include "base/strings/nullable_string16.h"
#include "content/common/page_state_serialization.h"
#include "content/public/common/page_state.h"
#include "content/renderer/history_entry.h"
#include "content/renderer/loader/web_url_request_util.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"

using blink::WebData;
using blink::WebHTTPBody;
using blink::WebHistoryItem;
using blink::WebSerializedScriptValue;
using blink::WebString;
using blink::WebVector;

namespace content {
namespace {

void ToOptionalString16Vector(
    const WebVector<WebString>& input,
    std::vector<base::Optional<base::string16>>* output) {
  output->reserve(output->size() + input.size());
  for (size_t i = 0; i < input.size(); ++i)
    output->emplace_back(WebString::ToOptionalString16(input[i]));
}

void GenerateFrameStateFromItem(const WebHistoryItem& item,
                                ExplodedFrameState* state) {
  state->url_string = WebString::ToOptionalString16(item.UrlString());
  state->referrer = WebString::ToOptionalString16(item.GetReferrer());
  state->referrer_policy = item.GetReferrerPolicy();
  state->target = WebString::ToOptionalString16(item.Target());
  if (!item.StateObject().IsNull()) {
    state->state_object =
        WebString::ToOptionalString16(item.StateObject().ToString());
  }
  state->scroll_restoration_type = item.ScrollRestorationType();
  state->visual_viewport_scroll_offset = item.VisualViewportScrollOffset();
  state->scroll_offset = item.GetScrollOffset();
  state->item_sequence_number = item.ItemSequenceNumber();
  state->document_sequence_number = item.DocumentSequenceNumber();
  state->page_scale_factor = item.PageScaleFactor();
  state->did_save_scroll_or_scale_state = item.DidSaveScrollOrScaleState();
  ToOptionalString16Vector(item.GetDocumentState(), &state->document_state);

  state->http_body.http_content_type =
      WebString::ToOptionalString16(item.HttpContentType());
  const WebHTTPBody& http_body = item.HttpBody();
  if (!http_body.IsNull()) {
    state->http_body.request_body = GetRequestBodyForWebHTTPBody(http_body);
    state->http_body.contains_passwords = http_body.ContainsPasswordData();
  }

  blink::ScrollAnchorData anchor = item.GetScrollAnchorData();
  state->scroll_anchor_selector =
      WebString::ToOptionalString16(anchor.selector_);
  state->scroll_anchor_offset = anchor.offset_;
  state->scroll_anchor_simhash = anchor.simhash_;
}

void RecursivelyGenerateHistoryItem(const ExplodedFrameState& state,
                                    HistoryEntry::HistoryNode* node) {
  WebHistoryItem item;
  item.Initialize();
  item.SetURLString(WebString::FromUTF16(state.url_string));
  item.SetReferrer(WebString::FromUTF16(state.referrer), state.referrer_policy);
  item.SetTarget(WebString::FromUTF16(state.target));
  if (state.state_object) {
    item.SetStateObject(WebSerializedScriptValue::FromString(
        WebString::FromUTF16(*state.state_object)));
  }
  WebVector<WebString> document_state(state.document_state.size());
  std::transform(state.document_state.begin(), state.document_state.end(),
                 document_state.begin(),
                 [](const base::Optional<base::string16>& s) {
                   return WebString::FromUTF16(s);
                 });
  item.SetDocumentState(document_state);
  item.SetScrollRestorationType(state.scroll_restoration_type);

  if (state.did_save_scroll_or_scale_state) {
    item.SetVisualViewportScrollOffset(state.visual_viewport_scroll_offset);
    item.SetScrollOffset(state.scroll_offset);
    item.SetPageScaleFactor(state.page_scale_factor);
  }

  // These values are generated at WebHistoryItem construction time, and we
  // only want to override those new values with old values if the old values
  // are defined.  A value of 0 means undefined in this context.
  if (state.item_sequence_number)
    item.SetItemSequenceNumber(state.item_sequence_number);
  if (state.document_sequence_number)
    item.SetDocumentSequenceNumber(state.document_sequence_number);

  item.SetHTTPContentType(
      WebString::FromUTF16(state.http_body.http_content_type));
  if (state.http_body.request_body != nullptr) {
    item.SetHttpBody(
        GetWebHTTPBodyForRequestBody(*state.http_body.request_body));
  }

  item.SetScrollAnchorData({WebString::FromUTF16(state.scroll_anchor_selector),
                            state.scroll_anchor_offset,
                            state.scroll_anchor_simhash});
  node->set_item(item);

  for (size_t i = 0; i < state.children.size(); ++i)
    RecursivelyGenerateHistoryItem(state.children[i], node->AddChild());
}

}  // namespace

PageState SingleHistoryItemToPageState(const WebHistoryItem& item) {
  ExplodedPageState state;
  ToOptionalString16Vector(item.GetReferencedFilePaths(),
                           &state.referenced_files);
  GenerateFrameStateFromItem(item, &state.top);

  std::string encoded_data;
  EncodePageState(state, &encoded_data);
  return PageState::CreateFromEncodedData(encoded_data);
}

std::unique_ptr<HistoryEntry> PageStateToHistoryEntry(
    const PageState& page_state) {
  ExplodedPageState state;
  if (!DecodePageState(page_state.ToEncodedData(), &state))
    return std::unique_ptr<HistoryEntry>();

  std::unique_ptr<HistoryEntry> entry(new HistoryEntry());
  RecursivelyGenerateHistoryItem(state.top, entry->root_history_node());

  return entry;
}

}  // namespace content
