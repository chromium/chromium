// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 *     (http://www.torchmobile.com/)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/web/web_history_entry.h"

#include <stddef.h>
#include <algorithm>
#include <memory>

#include "third_party/blink/public/common/page_state/page_state.h"
#include "third_party/blink/public/common/page_state/page_state_serialization.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_request_util.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"

namespace blink {

namespace {

WebVector<base::Optional<base::string16>> ToOptionalString16Vector(
    const WebVector<WebString>& input,
    WebVector<base::Optional<base::string16>> output) {
  output.reserve(output.size() + input.size());
  for (const auto& i : input)
    output.emplace_back(WebString::ToOptionalString16(i));
  return output;
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

  state->document_state =
      ToOptionalString16Vector(item.GetDocumentState(),
                               std::move(state->document_state))
          .ReleaseVector();

  state->http_body.http_content_type =
      WebString::ToOptionalString16(item.HttpContentType());
  const WebHTTPBody& http_body = item.HttpBody();
  if (!http_body.IsNull()) {
    state->http_body.request_body =
        blink::GetRequestBodyForWebHTTPBody(http_body);
    state->http_body.contains_passwords = http_body.ContainsPasswordData();
  }

  ScrollAnchorData anchor = item.GetScrollAnchorData();
  state->scroll_anchor_selector =
      WebString::ToOptionalString16(anchor.selector_);
  state->scroll_anchor_offset = anchor.offset_;
  state->scroll_anchor_simhash = anchor.simhash_;
}

void RecursivelyGenerateHistoryItem(const ExplodedFrameState& state,
                                    WebHistoryEntry::HistoryNode* node) {
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
  if (state.http_body.request_body) {
    item.SetHttpBody(
        blink::GetWebHTTPBodyForRequestBody(*state.http_body.request_body));
  }

  item.SetScrollAnchorData({WebString::FromUTF16(state.scroll_anchor_selector),
                            state.scroll_anchor_offset,
                            state.scroll_anchor_simhash});
  node->set_item(item);

  for (const auto& child : state.children)
    RecursivelyGenerateHistoryItem(child, node->AddChild());
}

}  // namespace

PageState SingleHistoryItemToPageState(const WebHistoryItem& item) {
  ExplodedPageState state;
  state.referenced_files =
      ToOptionalString16Vector(item.GetReferencedFilePaths(),
                               std::move(state.referenced_files))
          .ReleaseVector();
  GenerateFrameStateFromItem(item, &state.top);

  std::string encoded_data;
  EncodePageState(state, &encoded_data);
  return PageState::CreateFromEncodedData(encoded_data);
}

std::unique_ptr<WebHistoryEntry> PageStateToHistoryEntry(
    const PageState& page_state) {
  ExplodedPageState state;
  if (!DecodePageState(page_state.ToEncodedData(), &state))
    return std::unique_ptr<WebHistoryEntry>();

  std::unique_ptr<WebHistoryEntry> entry(new WebHistoryEntry());
  RecursivelyGenerateHistoryItem(state.top, entry->root_history_node());

  return entry;
}

WebHistoryEntry::HistoryNode* WebHistoryEntry::HistoryNode::AddChild(
    const WebHistoryItem& item) {
  children_.emplace_back(std::make_unique<HistoryNode>(entry_, item));
  return children_.back().get();
}

WebHistoryEntry::HistoryNode* WebHistoryEntry::HistoryNode::AddChild() {
  return AddChild(WebHistoryItem());
}

void WebHistoryEntry::HistoryNode::set_item(const WebHistoryItem& item) {
  DCHECK(!item.IsNull());
  item_ = item;
}

WebHistoryEntry::HistoryNode::HistoryNode(
    const base::WeakPtr<WebHistoryEntry>& entry,
    const WebHistoryItem& item)
    : entry_(entry) {
  if (!item.IsNull())
    set_item(item);
}

WebHistoryEntry::HistoryNode::~HistoryNode() = default;

WebVector<WebHistoryEntry::HistoryNode*>
WebHistoryEntry::HistoryNode::children() const {
  WebVector<WebHistoryEntry::HistoryNode*> children(children_.size());
  std::transform(children_.begin(), children_.end(), children.begin(),
                 [](const std::unique_ptr<WebHistoryEntry::HistoryNode>& item) {
                   return item.get();
                 });

  return children;
}

void WebHistoryEntry::HistoryNode::RemoveChildren() {
  children_.Clear();
}

WebHistoryEntry::WebHistoryEntry() {
  root_ = std::make_unique<HistoryNode>(weak_ptr_factory_.GetWeakPtr(),
                                        WebHistoryItem());
}

WebHistoryEntry::~WebHistoryEntry() = default;

WebHistoryEntry::WebHistoryEntry(const WebHistoryItem& root) {
  root_ = std::make_unique<HistoryNode>(weak_ptr_factory_.GetWeakPtr(), root);
}

}  // namespace blink
