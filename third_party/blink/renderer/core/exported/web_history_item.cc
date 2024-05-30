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

#include "third_party/blink/public/common/page_state/page_state.h"
#include "third_party/blink/public/common/page_state/page_state_serialization.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_request_util.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/loader/history_item.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace blink {

namespace {

WebVector<absl::optional<std::u16string>> ToOptionalString16Vector(
    const WebVector<WebString>& input,
    WebVector<absl::optional<std::u16string>> output) {
  output.reserve(output.size() + input.size());
  for (const auto& i : input)
    output.emplace_back(WebString::ToOptionalString16(i));
  return output;
}

}  // namespace

WebHistoryItem::WebHistoryItem(const PageState& page_state) {
  ExplodedPageState exploded_page_state;
  if (!DecodePageState(page_state.ToEncodedData(), &exploded_page_state))
    return;
  const ExplodedFrameState& state = exploded_page_state.top;

  private_ = MakeGarbageCollected<HistoryItem>();
  private_->SetURLString(WebString::FromUTF16(state.url_string));
  private_->SetReferrer(WebString::FromUTF16(state.referrer));
  private_->SetReferrerPolicy(state.referrer_policy);
  SetTarget(WebString::FromUTF16(state.target));
  if (state.state_object) {
    private_->SetStateObject(SerializedScriptValue::Create(
        WebString::FromUTF16(*state.state_object)));
  }

  Vector<String> document_state;
  for (auto& ds : state.document_state)
    document_state.push_back(WebString::FromUTF16(ds));
  private_->SetDocumentState(document_state);

  private_->SetScrollRestorationType(state.scroll_restoration_type);

  if (state.did_save_scroll_or_scale_state) {
    // TODO(crbug.com/1274078): Are these conversions from blink scroll offset
    // to gfx::PointF and gfx::Point correct?
    private_->SetVisualViewportScrollOffset(
        state.visual_viewport_scroll_offset.OffsetFromOrigin());
    private_->SetScrollOffset(
        ScrollOffset(state.scroll_offset.OffsetFromOrigin()));
    private_->SetPageScaleFactor(state.page_scale_factor);
  }

  // These values are generated at WebHistoryItem construction time, and we
  // only want to override those new values with old values if the old values
  // are defined.  A value of 0 means undefined in this context.
  if (state.item_sequence_number)
    private_->SetItemSequenceNumber(state.item_sequence_number);
  if (state.document_sequence_number)
    private_->SetDocumentSequenceNumber(state.document_sequence_number);
  if (state.navigation_api_key) {
    private_->SetNavigationApiKey(
        WebString::FromUTF16(state.navigation_api_key));
  }
  if (state.navigation_api_id)
    private_->SetNavigationApiId(WebString::FromUTF16(state.navigation_api_id));

  if (state.navigation_api_state) {
    private_->SetNavigationApiState(SerializedScriptValue::Create(
        WebString::FromUTF16(*state.navigation_api_state)));
  }

  private_->SetFormContentType(
      WebString::FromUTF16(state.http_body.http_content_type));
  if (state.http_body.request_body) {
    private_->SetFormData(
        blink::GetWebHTTPBodyForRequestBody(*state.http_body.request_body));
  }

  private_->SetScrollAnchorData(
      {WebString::FromUTF16(state.scroll_anchor_selector),
       state.scroll_anchor_offset, state.scroll_anchor_simhash});
}

WebHistoryItem::WebHistoryItem(const WebString& url,
                               const WebString& navigation_api_key,
                               const WebString& navigation_api_id,
                               int64_t item_sequence_number,
                               int64_t document_sequence_number,
                               const WebString& navigation_api_state) {
  private_ = MakeGarbageCollected<HistoryItem>();
  private_->SetURLString(url);
  private_->SetNavigationApiKey(navigation_api_key);
  private_->SetNavigationApiId(navigation_api_id);
  private_->SetItemSequenceNumber(item_sequence_number);
  private_->SetDocumentSequenceNumber(document_sequence_number);
  if (!navigation_api_state.IsNull()) {
    private_->SetNavigationApiState(
        SerializedScriptValue::Create(navigation_api_state));
  }
}

PageState WebHistoryItem::ToPageState() {
  ExplodedPageState state;
  state.referenced_files =
      ToOptionalString16Vector(GetReferencedFilePaths(),
                               std::move(state.referenced_files))
          .ReleaseVector();

  state.top.url_string = WebString::ToOptionalString16(private_->UrlString());
  state.top.referrer = WebString::ToOptionalString16(private_->GetReferrer());
  state.top.referrer_policy = private_->GetReferrerPolicy();
  state.top.target = WebString::ToOptionalString16(target_);
  if (private_->StateObject()) {
    state.top.state_object =
        WebString::ToOptionalString16(private_->StateObject()->ToWireString());
  }
  state.top.scroll_restoration_type = private_->ScrollRestorationType();

  if (const auto& scroll_and_view_state = private_->GetViewState()) {
    // TODO(crbug.com/1274078): Are these conversions from blink scroll offset
    // to gfx::PointF and gfx::Point correct?
    state.top.visual_viewport_scroll_offset = gfx::PointAtOffsetFromOrigin(
        scroll_and_view_state->visual_viewport_scroll_offset_);
    state.top.scroll_offset = gfx::ToFlooredPoint(
        gfx::PointAtOffsetFromOrigin(scroll_and_view_state->scroll_offset_));
    state.top.page_scale_factor = scroll_and_view_state->page_scale_factor_;
    state.top.did_save_scroll_or_scale_state = true;
  } else {
    state.top.visual_viewport_scroll_offset = gfx::PointF();
    state.top.scroll_offset = gfx::Point();
    state.top.page_scale_factor = 0;
    state.top.did_save_scroll_or_scale_state = false;
  }

  state.top.item_sequence_number = ItemSequenceNumber();
  state.top.document_sequence_number = DocumentSequenceNumber();

  state.top.document_state =
      ToOptionalString16Vector(private_->GetDocumentState(),
                               std::move(state.top.document_state))
          .ReleaseVector();

  state.top.http_body.http_content_type =
      WebString::ToOptionalString16(private_->FormContentType());
  const WebHTTPBody& http_body = HttpBody();
  if (!http_body.IsNull()) {
    state.top.http_body.request_body =
        blink::GetRequestBodyForWebHTTPBody(http_body);
    state.top.http_body.contains_passwords = http_body.ContainsPasswordData();
  }

  ScrollAnchorData anchor;
  if (private_->GetViewState())
    anchor = private_->GetViewState()->scroll_anchor_data_;
  state.top.scroll_anchor_selector =
      WebString::ToOptionalString16(anchor.selector_);
  state.top.scroll_anchor_offset = anchor.offset_;
  state.top.scroll_anchor_simhash = anchor.simhash_;

  state.top.navigation_api_key =
      WebString::ToOptionalString16(private_->GetNavigationApiKey());
  state.top.navigation_api_id =
      WebString::ToOptionalString16(private_->GetNavigationApiId());
  if (private_->GetNavigationApiState()) {
    state.top.navigation_api_state = WebString::ToOptionalString16(
        private_->GetNavigationApiState()->ToWireString());
  }

  std::string encoded_data;
  EncodePageState(state, &encoded_data);
  return PageState::CreateFromEncodedData(encoded_data);
}

void WebHistoryItem::Reset() {
  private_.Reset();
  target_.Reset();
}

void WebHistoryItem::Assign(const WebHistoryItem& other) {
  private_ = other.private_;
  target_ = other.target_;
}

void WebHistoryItem::SetTarget(const WebString& target) {
  target_ = target;
}

int64_t WebHistoryItem::ItemSequenceNumber() const {
  return private_->ItemSequenceNumber();
}

int64_t WebHistoryItem::DocumentSequenceNumber() const {
  return private_->DocumentSequenceNumber();
}

WebHTTPBody WebHistoryItem::HttpBody() const {
  return WebHTTPBody(private_->FormData());
}

WebVector<WebString> WebHistoryItem::GetReferencedFilePaths() const {
  HashSet<String> file_paths;
  const EncodedFormData* form_data = private_->FormData();
  if (form_data) {
    for (wtf_size_t i = 0; i < form_data->Elements().size(); ++i) {
      const FormDataElement& element = form_data->Elements()[i];
      if (element.type_ == FormDataElement::kEncodedFile)
        file_paths.insert(element.filename_);
    }
  }

  const Vector<String>& referenced_file_paths =
      private_->GetReferencedFilePaths();
  for (wtf_size_t i = 0; i < referenced_file_paths.size(); ++i)
    file_paths.insert(referenced_file_paths[i]);

  Vector<String> results(file_paths);
  return results;
}

WebString WebHistoryItem::GetNavigationApiKey() const {
  return private_->GetNavigationApiKey();
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
