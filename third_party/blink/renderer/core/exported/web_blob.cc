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

#include "third_party/blink/public/web/web_blob.h"

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_blob.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/file_metadata.h"

namespace blink {

WebBlob WebBlob::CreateFromUUID(const WebString& uuid,
                                const WebString& type,
                                uint64_t size) {
  return Blob::Create(BlobDataHandle::Create(uuid, type, size));
}

WebBlob WebBlob::CreateFromFile(const WebString& path, uint64_t size) {
  auto blob_data = std::make_unique<BlobData>();
  blob_data->AppendFile(path, 0, size, InvalidFileTime());
  return Blob::Create(BlobDataHandle::Create(std::move(blob_data), size));
}

WebBlob WebBlob::FromV8Value(v8::Local<v8::Value> value) {
  if (V8Blob::HasInstance(value, v8::Isolate::GetCurrent())) {
    v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
    Blob* blob = V8Blob::ToImpl(object);
    DCHECK(blob);
    return blob;
  }
  return WebBlob();
}

void WebBlob::Reset() {
  private_.Reset();
}

void WebBlob::Assign(const WebBlob& other) {
  private_ = other.private_;
}

WebString WebBlob::Uuid() {
  if (!private_.Get())
    return WebString();
  return private_->Uuid();
}

v8::Local<v8::Value> WebBlob::ToV8Value(v8::Local<v8::Object> creation_context,
                                        v8::Isolate* isolate) {
  // We no longer use |creationContext| because it's often misused and points
  // to a context faked by user script.
  DCHECK(creation_context->CreationContext() == isolate->GetCurrentContext());
  if (!private_.Get())
    return v8::Local<v8::Value>();
  return ToV8(private_.Get(), isolate->GetCurrentContext()->Global(), isolate);
}

WebBlob::WebBlob(Blob* blob) : private_(blob) {}

WebBlob& WebBlob::operator=(Blob* blob) {
  private_ = blob;
  return *this;
}

}  // namespace blink
