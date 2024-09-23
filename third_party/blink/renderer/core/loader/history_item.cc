/*
 * Copyright (C) 2005, 2006, 2008, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/loader/history_item.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "third_party/blink/public/common/page_state/page_state_serialization.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_url_request_util.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace blink {

namespace {

std::vector<std::optional<std::u16string>> ToOptionalString16Vector(
    base::span<const String> input) {
  std::vector<std::optional<std::u16string>> output;
  output.reserve(input.size());
  for (const auto& i : input) {
    output.emplace_back(WebString::ToOptionalString16(i));
  }
  return output;
}

}  // namespace

static int64_t GenerateSequenceNumber() {
  // Initialize to the current time to reduce the likelihood of generating
  // identifiers that overlap with those from past/future browser sessions.
  static int64_t next =
      (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds();
  return ++next;
}

HistoryItem* HistoryItem::Create(const PageState& page_state) {
  ExplodedPageState exploded_page_state;
  if (!DecodePageState(page_state.ToEncodedData(), &exploded_page_state)) {
    return nullptr;
  }

  auto* new_item = MakeGarbageCollected<HistoryItem>();
  const ExplodedFrameState& state = exploded_page_state.top;
  new_item->SetURLString(WebString::FromUTF16(state.url_string));
  new_item->SetReferrer(WebString::FromUTF16(state.referrer));
  new_item->SetReferrerPolicy(state.referrer_policy);
  new_item->SetTarget(WebString::FromUTF16(state.target));
  if (state.state_object) {
    new_item->SetStateObject(SerializedScriptValue::Create(
        WebString::FromUTF16(*state.state_object)));
  }

  Vector<String> document_state;
  for (auto& ds : state.document_state) {
    document_state.push_back(WebString::FromUTF16(ds));
  }
  new_item->SetDocumentState(document_state);

  new_item->SetScrollRestorationType(state.scroll_restoration_type);

  if (state.did_save_scroll_or_scale_state) {
    // TODO(crbug.com/1274078): Are these conversions from blink scroll offset
    // to gfx::PointF and gfx::Point correct?
    new_item->SetVisualViewportScrollOffset(
        state.visual_viewport_scroll_offset.OffsetFromOrigin());
    new_item->SetScrollOffset(
        ScrollOffset(state.scroll_offset.OffsetFromOrigin()));
    new_item->SetPageScaleFactor(state.page_scale_factor);
  }

  // These values are generated at HistoryItem construction time, and we only
  // want to override those new values with old values if the old values are
  // defined. A value of 0 means undefined in this context.
  if (state.item_sequence_number) {
    new_item->SetItemSequenceNumber(state.item_sequence_number);
  }
  if (state.document_sequence_number) {
    new_item->SetDocumentSequenceNumber(state.document_sequence_number);
  }
  if (state.navigation_api_key) {
    new_item->SetNavigationApiKey(
        WebString::FromUTF16(state.navigation_api_key));
  }
  if (state.navigation_api_id) {
    new_item->SetNavigationApiId(WebString::FromUTF16(state.navigation_api_id));
  }

  if (state.navigation_api_state) {
    new_item->SetNavigationApiState(SerializedScriptValue::Create(
        WebString::FromUTF16(*state.navigation_api_state)));
  }

  new_item->SetFormContentType(
      WebString::FromUTF16(state.http_body.http_content_type));
  if (state.http_body.request_body) {
    new_item->SetFormData(
        blink::GetWebHTTPBodyForRequestBody(*state.http_body.request_body));
  }

  new_item->SetScrollAnchorData(
      {WebString::FromUTF16(state.scroll_anchor_selector),
       state.scroll_anchor_offset, state.scroll_anchor_simhash});
  return new_item;
}

HistoryItem::HistoryItem()
    : item_sequence_number_(GenerateSequenceNumber()),
      document_sequence_number_(GenerateSequenceNumber()),
      navigation_api_key_(WTF::CreateCanonicalUUIDString()),
      navigation_api_id_(WTF::CreateCanonicalUUIDString()) {}

HistoryItem::~HistoryItem() = default;

const String& HistoryItem::UrlString() const {
  return url_string_;
}

KURL HistoryItem::Url() const {
  return KURL(url_string_);
}

const String& HistoryItem::GetReferrer() const {
  return referrer_;
}

network::mojom::ReferrerPolicy HistoryItem::GetReferrerPolicy() const {
  return referrer_policy_;
}

void HistoryItem::SetURLString(const String& url_string) {
  if (url_string_ != url_string)
    url_string_ = url_string;
}

void HistoryItem::SetURL(const KURL& url) {
  SetURLString(url.GetString());
}

void HistoryItem::SetReferrer(const String& referrer) {
  referrer_ = referrer;
}

void HistoryItem::SetReferrerPolicy(network::mojom::ReferrerPolicy policy) {
  referrer_policy_ = policy;
}

HistoryItem::ViewState& HistoryItem::GetOrCreateViewState() {
  if (!view_state_) {
    view_state_ = ViewState();
  }
  return *view_state_;
}

void HistoryItem::SetVisualViewportScrollOffset(const ScrollOffset& offset) {
  GetOrCreateViewState().visual_viewport_scroll_offset_ = offset;
}

void HistoryItem::SetScrollOffset(const ScrollOffset& offset) {
  GetOrCreateViewState().scroll_offset_ = offset;
}

void HistoryItem::SetPageScaleFactor(float scale_factor) {
  GetOrCreateViewState().page_scale_factor_ = scale_factor;
}

void HistoryItem::SetScrollAnchorData(
    const ScrollAnchorData& scroll_anchor_data) {
  GetOrCreateViewState().scroll_anchor_data_ = scroll_anchor_data;
}

void HistoryItem::SetDocumentState(const Vector<String>& state) {
  DCHECK(!document_state_);
  document_state_vector_ = state;
}

void HistoryItem::SetDocumentState(DocumentState* state) {
  document_state_ = state;
}

const Vector<String>& HistoryItem::GetDocumentState() const {
  // TODO(dcheng): This is super weird. It seems like it would be better to just
  // populate the vector eagerly once when calling `SetDocumentState()` with a
  // `DocumentState` object.
  if (document_state_)
    document_state_vector_ = document_state_->ToStateVector();
  return document_state_vector_;
}

Vector<String> HistoryItem::GetReferencedFilePaths() const {
  return FormController::GetReferencedFilePaths(GetDocumentState());
}

void HistoryItem::ClearDocumentState() {
  document_state_.Clear();
  document_state_vector_.clear();
}

void HistoryItem::SetStateObject(scoped_refptr<SerializedScriptValue> object) {
  state_object_ = std::move(object);
}

const AtomicString& HistoryItem::FormContentType() const {
  return form_content_type_;
}

void HistoryItem::SetFormData(scoped_refptr<EncodedFormData> form_data) {
  form_data_ = std::move(form_data);
}

void HistoryItem::SetFormContentType(const AtomicString& form_content_type) {
  form_content_type_ = form_content_type;
}

EncodedFormData* HistoryItem::FormData() const {
  return form_data_.get();
}

void HistoryItem::SetNavigationApiState(
    scoped_refptr<SerializedScriptValue> value) {
  navigation_api_state_ = std::move(value);
}

ResourceRequest HistoryItem::GenerateResourceRequest(
    mojom::FetchCacheMode cache_mode) {
  ResourceRequest request(url_string_);
  request.SetReferrerString(referrer_);
  request.SetReferrerPolicy(referrer_policy_);
  request.SetCacheMode(cache_mode);
  if (form_data_) {
    request.SetHttpMethod(http_names::kPOST);
    request.SetHttpBody(form_data_);
    request.SetHTTPContentType(form_content_type_);
    request.SetHTTPOriginToMatchReferrerIfNeeded();
  }
  return request;
}

void HistoryItem::Trace(Visitor* visitor) const {
  visitor->Trace(document_state_);
}

PageState HistoryItem::ToPageState() const {
  ExplodedPageState state;
  state.referenced_files = GetReferencedFilePathsForSerialization();

  state.top.url_string = WebString::ToOptionalString16(UrlString());
  state.top.referrer = WebString::ToOptionalString16(GetReferrer());
  state.top.referrer_policy = GetReferrerPolicy();
  state.top.target = WebString::ToOptionalString16(Target());
  if (StateObject()) {
    state.top.state_object =
        WebString::ToOptionalString16(StateObject()->ToWireString());
  }
  state.top.scroll_restoration_type = ScrollRestorationType();

  ScrollAnchorData anchor;
  if (const auto& scroll_and_view_state = GetViewState()) {
    // TODO(crbug.com/1274078): Are these conversions from blink scroll offset
    // to gfx::PointF and gfx::Point correct?
    state.top.visual_viewport_scroll_offset = gfx::PointAtOffsetFromOrigin(
        scroll_and_view_state->visual_viewport_scroll_offset_);
    state.top.scroll_offset = gfx::ToFlooredPoint(
        gfx::PointAtOffsetFromOrigin(scroll_and_view_state->scroll_offset_));
    state.top.page_scale_factor = scroll_and_view_state->page_scale_factor_;
    state.top.did_save_scroll_or_scale_state = true;
    anchor = scroll_and_view_state->scroll_anchor_data_;
  } else {
    state.top.visual_viewport_scroll_offset = gfx::PointF();
    state.top.scroll_offset = gfx::Point();
    state.top.page_scale_factor = 0;
    state.top.did_save_scroll_or_scale_state = false;
  }

  state.top.scroll_anchor_selector =
      WebString::ToOptionalString16(anchor.selector_);
  state.top.scroll_anchor_offset = anchor.offset_;
  state.top.scroll_anchor_simhash = anchor.simhash_;

  state.top.item_sequence_number = ItemSequenceNumber();
  state.top.document_sequence_number = DocumentSequenceNumber();

  state.top.document_state = ToOptionalString16Vector(GetDocumentState());

  state.top.http_body.http_content_type =
      WebString::ToOptionalString16(FormContentType());
  WebHTTPBody http_body(FormData());
  if (!http_body.IsNull()) {
    state.top.http_body.request_body =
        blink::GetRequestBodyForWebHTTPBody(http_body);
    state.top.http_body.contains_passwords = http_body.ContainsPasswordData();
  }

  state.top.navigation_api_key =
      WebString::ToOptionalString16(GetNavigationApiKey());
  state.top.navigation_api_id =
      WebString::ToOptionalString16(GetNavigationApiId());
  if (GetNavigationApiState()) {
    state.top.navigation_api_state =
        WebString::ToOptionalString16(GetNavigationApiState()->ToWireString());
  }

  std::string encoded_data;
  EncodePageState(state, &encoded_data);
  return PageState::CreateFromEncodedData(encoded_data);
}

std::vector<std::optional<std::u16string>>
HistoryItem::GetReferencedFilePathsForSerialization() const {
  HashSet<String> file_paths;

  // These additional paths are presumably used by PageState so the browser can
  // grant the renderer access to referenced files during session restore. This
  // logic dates to https://crrev.com/db4a9b4108635b3678c3f9fd5bdd1f98001db216,
  // and it is not entirely clear if it is still needed.
  const EncodedFormData* form_data = FormData();
  if (form_data) {
    for (const FormDataElement& element : form_data->Elements()) {
      if (element.type_ == FormDataElement::kEncodedFile) {
        file_paths.insert(element.filename_);
      }
    }
  }

  for (const String& path : GetReferencedFilePaths()) {
    file_paths.insert(path);
  }

  std::vector<std::optional<std::u16string>> result;
  result.reserve(file_paths.size());
  base::ranges::transform(file_paths, std::back_inserter(result),
                          WebString::ToOptionalString16);
  return result;
}

}  // namespace blink
