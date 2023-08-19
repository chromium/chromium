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

#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/loader/history_item.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

WebHistoryItem::WebHistoryItem(const PageState& page_state) {
  private_ = HistoryItem::Create(page_state);
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

void WebHistoryItem::Reset() {
  private_.Reset();
}

void WebHistoryItem::Assign(const WebHistoryItem& other) {
  private_ = other.private_;
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
