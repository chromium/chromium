/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "third_party/blink/public/platform/modules/indexeddb/web_idb_key.h"

#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

size_t WebIDBKeyArrayView::size() const {
  return private_->Array().size();
}

WebIDBKeyView WebIDBKeyArrayView::operator[](size_t index) const {
  return WebIDBKeyView(private_->Array()[SafeCast<wtf_size_t>(index)].get());
}

WebIDBKeyType WebIDBKeyView::KeyType() const {
  if (!private_)
    return kWebIDBKeyTypeNull;
  return static_cast<WebIDBKeyType>(private_->GetType());
}

bool WebIDBKeyView::IsValid() const {
  if (!private_)
    return false;
  return private_->IsValid();
}

WebData WebIDBKeyView::Binary() const {
  return private_->Binary();
}

WebString WebIDBKeyView::String() const {
  return private_->GetString();
}

double WebIDBKeyView::Date() const {
  return private_->Date();
}

double WebIDBKeyView::Number() const {
  return private_->Number();
}

size_t WebIDBKeyView::SizeEstimate() const {
  // TODO(cmp): Ensure |private_| can never be null.
  //
  // SizeEstimate() can be called when |private_| is null.  That happens if and
  // only if the |WebIDBKey| instance is created using WebIDBKey::CreateNull().
  //
  // Eventually, WebIDBKey::CreateNull() will change so that case will lead to
  // a non-null |private_|.  At that time, this null check can change to a
  // DCHECK that |private_| is not null and the special null case handling can
  // be removed.
  if (this->IsNull()) {
    return IDBKey::kIDBKeyOverheadSize;
  }

  return private_->SizeEstimate();
}

WebIDBKey WebIDBKey::CreateArray(WebVector<WebIDBKey> array) {
  IDBKey::KeyArray keys;
  keys.ReserveCapacity(SafeCast<wtf_size_t>(array.size()));
  for (WebIDBKey& key : array) {
    DCHECK(key.View().KeyType() != kWebIDBKeyTypeNull);
    keys.emplace_back(key.ReleaseIdbKey());
  }
  return WebIDBKey(IDBKey::CreateArray(std::move(keys)));
}

WebIDBKey WebIDBKey::CreateBinary(const WebData& binary) {
  return WebIDBKey(IDBKey::CreateBinary(binary));
}

WebIDBKey WebIDBKey::CreateString(const WebString& string) {
  return WebIDBKey(IDBKey::CreateString(string));
}

WebIDBKey WebIDBKey::CreateDate(double date) {
  return WebIDBKey(IDBKey::CreateDate(date));
}

WebIDBKey WebIDBKey::CreateNumber(double number) {
  return WebIDBKey(IDBKey::CreateNumber(number));
}

WebIDBKey WebIDBKey::CreateInvalid() {
  return WebIDBKey(IDBKey::CreateInvalid());
}

WebIDBKey::WebIDBKey() noexcept = default;

WebIDBKey::WebIDBKey(WebIDBKey&&) noexcept = default;
WebIDBKey& WebIDBKey::operator=(WebIDBKey&&) noexcept = default;

WebIDBKey::~WebIDBKey() noexcept = default;

WebIDBKey::WebIDBKey(std::unique_ptr<IDBKey> idb_key) noexcept
    : private_(std::move(idb_key)) {}
WebIDBKey& WebIDBKey::operator=(std::unique_ptr<IDBKey> idb_key) noexcept {
  private_ = std::move(idb_key);
  return *this;
}

WebIDBKey::WebIDBKey(const WebIDBKey& rkey)
    : private_(IDBKey::Clone(rkey.private_)) {}
WebIDBKey& WebIDBKey::operator=(const WebIDBKey& rkey) {
  private_ = IDBKey::Clone(rkey.private_);
  return *this;
}

}  // namespace blink
