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

#include "third_party/blink/public/platform/web_data.h"

#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

#include <vector>

namespace blink {

void WebData::Reset() {
  private_.Reset();
}

void WebData::Assign(const WebData& other) {
  private_ = other.private_;
}

void WebData::Assign(const char* data, size_t size) {
  private_ = SharedBuffer::Create(data, size);
}

void WebData::Append(const char* data, size_t size) {
  if (private_.IsNull())
    private_ = SharedBuffer::Create(data, size);
  else
    private_->Append(data, size);
}

size_t WebData::size() const {
  if (private_.IsNull())
    return 0;
  return private_->size();
}

size_t WebData::GetSomeData(const char*& data, size_t position) const {
  data = nullptr;
  if (private_.IsNull())
    return 0;
  const auto it = private_->GetIteratorAt(position);
  if (it == private_->cend())
    return 0;
  data = it->data();
  return it->size();
}

WebVector<uint8_t> WebData::Copy() const {
  return private_.IsNull()
             ? WebVector<uint8_t>()
             : WebVector<uint8_t>(private_->CopyAs<std::vector<uint8_t>>());
}

WebData::WebData(scoped_refptr<SharedBuffer> buffer)
    : private_(std::move(buffer)) {}

WebData& WebData::operator=(scoped_refptr<SharedBuffer> buffer) {
  private_ = std::move(buffer);
  return *this;
}

WebData::operator scoped_refptr<SharedBuffer>() const {
  return scoped_refptr<SharedBuffer>(private_.Get());
}

WebData::operator const SharedBuffer&() const {
  return *private_;
}

}  // namespace blink
