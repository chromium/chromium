/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/web/web_history_item.h"

#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/loader/history_item.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

void WebHistoryItem::Initialize() {
  private_ = MakeGarbageCollected<HistoryItem>();
}

void WebHistoryItem::Reset() {
  private_.Reset();
  target_.Reset();
}

void WebHistoryItem::Assign(const WebHistoryItem& other) {
  private_ = other.private_;
  target_ = other.target_;
}

WebString WebHistoryItem::UrlString() const {
  return private_->UrlString();
}

void WebHistoryItem::SetURLString(const WebString& url) {
  private_->SetURLString(KURL(url).GetString());
}

WebString WebHistoryItem::GetReferrer() const {
  return private_->GetReferrer().referrer;
}

network::mojom::ReferrerPolicy WebHistoryItem::GetReferrerPolicy() const {
  return private_->GetReferrer().referrer_policy;
}

void WebHistoryItem::SetReferrer(
    const WebString& referrer,
    network::mojom::ReferrerPolicy referrer_policy) {
  private_->SetReferrer(Referrer(referrer, referrer_policy));
}

const WebString& WebHistoryItem::Target() const {
  return target_;
}

void WebHistoryItem::SetTarget(const WebString& target) {
  target_ = target;
}

WebFloatPoint WebHistoryItem::VisualViewportScrollOffset() const {
  const auto& scroll_and_view_state = private_->GetViewState();
  ScrollOffset offset =
      scroll_and_view_state
          ? scroll_and_view_state->visual_viewport_scroll_offset_
          : ScrollOffset();
  return WebFloatPoint(offset.Width(), offset.Height());
}

void WebHistoryItem::SetVisualViewportScrollOffset(
    const WebFloatPoint& scroll_offset) {
  private_->SetVisualViewportScrollOffset(ToScrollOffset(scroll_offset));
}

WebPoint WebHistoryItem::GetScrollOffset() const {
  const auto& scroll_and_view_state = private_->GetViewState();
  ScrollOffset offset = scroll_and_view_state
                            ? scroll_and_view_state->scroll_offset_
                            : ScrollOffset();
  return WebPoint(offset.Width(), offset.Height());
}

void WebHistoryItem::SetScrollOffset(const WebPoint& scroll_offset) {
  private_->SetScrollOffset(ScrollOffset(scroll_offset.x, scroll_offset.y));
}

float WebHistoryItem::PageScaleFactor() const {
  const auto& scroll_and_view_state = private_->GetViewState();
  return scroll_and_view_state ? scroll_and_view_state->page_scale_factor_ : 0;
}

void WebHistoryItem::SetPageScaleFactor(float scale) {
  private_->SetPageScaleFactor(scale);
}

WebVector<WebString> WebHistoryItem::GetDocumentState() const {
  return private_->GetDocumentState();
}

void WebHistoryItem::SetDocumentState(const WebVector<WebString>& state) {
  // FIXME: would be nice to avoid the intermediate copy
  Vector<String> ds;
  for (size_t i = 0; i < state.size(); ++i)
    ds.push_back(state[i]);
  private_->SetDocumentState(ds);
}

int64_t WebHistoryItem::ItemSequenceNumber() const {
  return private_->ItemSequenceNumber();
}

void WebHistoryItem::SetItemSequenceNumber(int64_t item_sequence_number) {
  private_->SetItemSequenceNumber(item_sequence_number);
}

int64_t WebHistoryItem::DocumentSequenceNumber() const {
  return private_->DocumentSequenceNumber();
}

void WebHistoryItem::SetDocumentSequenceNumber(
    int64_t document_sequence_number) {
  private_->SetDocumentSequenceNumber(document_sequence_number);
}

WebHistoryScrollRestorationType WebHistoryItem::ScrollRestorationType() const {
  return static_cast<WebHistoryScrollRestorationType>(
      private_->ScrollRestorationType());
}

void WebHistoryItem::SetScrollRestorationType(
    WebHistoryScrollRestorationType type) {
  private_->SetScrollRestorationType(
      static_cast<HistoryScrollRestorationType>(type));
}

WebSerializedScriptValue WebHistoryItem::StateObject() const {
  return WebSerializedScriptValue(private_->StateObject());
}

void WebHistoryItem::SetStateObject(const WebSerializedScriptValue& object) {
  private_->SetStateObject(object);
}

WebString WebHistoryItem::HttpContentType() const {
  return private_->FormContentType();
}

void WebHistoryItem::SetHTTPContentType(const WebString& http_content_type) {
  private_->SetFormContentType(http_content_type);
}

WebHTTPBody WebHistoryItem::HttpBody() const {
  return WebHTTPBody(private_->FormData());
}

void WebHistoryItem::SetHttpBody(const WebHTTPBody& http_body) {
  private_->SetFormData(http_body);
}

WebVector<WebString> WebHistoryItem::GetReferencedFilePaths() const {
  HashSet<String> file_paths;
  const EncodedFormData* form_data = private_->FormData();
  if (form_data) {
    for (size_t i = 0; i < form_data->Elements().size(); ++i) {
      const FormDataElement& element = form_data->Elements()[i];
      if (element.type_ == FormDataElement::kEncodedFile)
        file_paths.insert(element.filename_);
    }
  }

  const Vector<String>& referenced_file_paths =
      private_->GetReferencedFilePaths();
  for (size_t i = 0; i < referenced_file_paths.size(); ++i)
    file_paths.insert(referenced_file_paths[i]);

  Vector<String> results;
  CopyToVector(file_paths, results);
  return results;
}

bool WebHistoryItem::DidSaveScrollOrScaleState() const {
  return private_->GetViewState().has_value();
}

ScrollAnchorData WebHistoryItem::GetScrollAnchorData() const {
  if (private_->GetViewState()) {
    return private_->GetViewState()->scroll_anchor_data_;
  }

  return ScrollAnchorData();
}

void WebHistoryItem::SetScrollAnchorData(
    const struct ScrollAnchorData& scroll_anchor_data) {
  private_->SetScrollAnchorData(scroll_anchor_data);
}

WebHistoryItem::WebHistoryItem(HistoryItem* item) : private_(item) {}

WebHistoryItem& WebHistoryItem::operator=(HistoryItem* item) {
  private_ = item;
  return *this;
}

WebHistoryItem::operator HistoryItem*() const {
  return private_.Get();
}

}  // namespace blink
