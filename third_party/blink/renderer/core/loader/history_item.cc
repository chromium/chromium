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
#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

static int64_t GenerateSequenceNumber() {
  // Initialize to the current time to reduce the likelihood of generating
  // identifiers that overlap with those from past/future browser sessions.
  static int64_t next =
      static_cast<int64_t>(base::Time::Now().ToDoubleT() * 1000000.0);
  return ++next;
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

void HistoryItem::SetVisualViewportScrollOffset(const ScrollOffset& offset) {
  if (!view_state_)
    view_state_ = absl::make_optional<ViewState>();
  view_state_->visual_viewport_scroll_offset_ = offset;
}

void HistoryItem::SetScrollOffset(const ScrollOffset& offset) {
  if (!view_state_)
    view_state_ = absl::make_optional<ViewState>();
  view_state_->scroll_offset_ = offset;
}

void HistoryItem::SetPageScaleFactor(float scale_factor) {
  if (!view_state_)
    view_state_ = absl::make_optional<ViewState>();
  view_state_->page_scale_factor_ = scale_factor;
}

void HistoryItem::SetScrollAnchorData(
    const ScrollAnchorData& scroll_anchor_data) {
  if (!view_state_)
    view_state_ = absl::make_optional<ViewState>();
  view_state_->scroll_anchor_data_ = scroll_anchor_data;
}

void HistoryItem::SetDocumentState(const Vector<String>& state) {
  DCHECK(!document_state_);
  document_state_vector_ = state;
}

void HistoryItem::SetDocumentState(DocumentState* state) {
  document_state_ = state;
}

const Vector<String>& HistoryItem::GetDocumentState() {
  if (document_state_)
    document_state_vector_ = document_state_->ToStateVector();
  return document_state_vector_;
}

Vector<String> HistoryItem::GetReferencedFilePaths() {
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

EncodedFormData* HistoryItem::FormData() {
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

}  // namespace blink
