/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/public/web/web_dom_file_system.h"

#include "third_party/blink/public/mojom/filesystem/file_system.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_directory_entry.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_dom_file_system.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_entry.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_entry.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/modules/filesystem/directory_entry.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system.h"
#include "third_party/blink/renderer/modules/filesystem/file_entry.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "v8/include/v8.h"

namespace blink {

WebDOMFileSystem WebDOMFileSystem::FromV8Value(v8::Local<v8::Value> value) {
  if (!V8DOMFileSystem::HasInstance(value, v8::Isolate::GetCurrent()))
    return WebDOMFileSystem();
  v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
  DOMFileSystem* dom_file_system = V8DOMFileSystem::ToImpl(object);
  DCHECK(dom_file_system);
  return WebDOMFileSystem(dom_file_system);
}

WebURL WebDOMFileSystem::CreateFileSystemURL(v8::Local<v8::Value> value) {
  const Entry* const entry =
      V8Entry::ToImplWithTypeCheck(v8::Isolate::GetCurrent(), value);
  if (entry)
    return entry->filesystem()->CreateFileSystemURL(entry);
  return WebURL();
}

WebDOMFileSystem WebDOMFileSystem::Create(WebLocalFrame* frame,
                                          WebFileSystemType type,
                                          const WebString& name,
                                          const WebURL& root_url,
                                          SerializableType serializable_type) {
  DCHECK(frame);
  DCHECK(To<WebLocalFrameImpl>(frame)->GetFrame());
  auto* dom_file_system = MakeGarbageCollected<DOMFileSystem>(
      To<WebLocalFrameImpl>(frame)->GetFrame()->DomWindow(), name,
      static_cast<mojom::blink::FileSystemType>(type), root_url);
  if (serializable_type == kSerializableTypeSerializable)
    dom_file_system->MakeClonable();
  return WebDOMFileSystem(dom_file_system);
}

void WebDOMFileSystem::Reset() {
  private_.Reset();
}

void WebDOMFileSystem::Assign(const WebDOMFileSystem& other) {
  private_ = other.private_;
}

WebString WebDOMFileSystem::GetName() const {
  DCHECK(private_.Get());
  return private_->name();
}

WebFileSystemType WebDOMFileSystem::GetType() const {
  DCHECK(private_.Get());
  switch (private_->GetType()) {
    case blink::mojom::FileSystemType::kTemporary:
      return WebFileSystemType::kWebFileSystemTypeTemporary;
    case blink::mojom::FileSystemType::kPersistent:
      return WebFileSystemType::kWebFileSystemTypePersistent;
    case blink::mojom::FileSystemType::kIsolated:
      return WebFileSystemType::kWebFileSystemTypeIsolated;
    case blink::mojom::FileSystemType::kExternal:
      return WebFileSystemType::kWebFileSystemTypeExternal;
    default:
      NOTREACHED();
      return WebFileSystemType::kWebFileSystemTypeTemporary;
  }
}

WebURL WebDOMFileSystem::RootURL() const {
  DCHECK(private_.Get());
  return private_->RootURL();
}

v8::Local<v8::Value> WebDOMFileSystem::ToV8Value(
    v8::Local<v8::Object> creation_context,
    v8::Isolate* isolate) {
  // We no longer use |creationContext| because it's often misused and points
  // to a context faked by user script.
  DCHECK(creation_context->CreationContext() == isolate->GetCurrentContext());
  if (!private_.Get())
    return v8::Local<v8::Value>();
  return ToV8(private_.Get(), isolate->GetCurrentContext()->Global(), isolate);
}

v8::Local<v8::Value> WebDOMFileSystem::CreateV8Entry(
    const WebString& path,
    EntryType entry_type,
    v8::Local<v8::Object> creation_context,
    v8::Isolate* isolate) {
  // We no longer use |creationContext| because it's often misused and points
  // to a context faked by user script.
  DCHECK(creation_context->CreationContext() == isolate->GetCurrentContext());
  if (!private_.Get())
    return v8::Local<v8::Value>();
  if (entry_type == kEntryTypeDirectory) {
    return ToV8(MakeGarbageCollected<DirectoryEntry>(private_.Get(), path),
                isolate->GetCurrentContext()->Global(), isolate);
  }
  DCHECK_EQ(entry_type, kEntryTypeFile);
  return ToV8(MakeGarbageCollected<FileEntry>(private_.Get(), path),
              isolate->GetCurrentContext()->Global(), isolate);
}

WebDOMFileSystem::WebDOMFileSystem(DOMFileSystem* dom_file_system)
    : private_(dom_file_system) {}

WebDOMFileSystem& WebDOMFileSystem::operator=(DOMFileSystem* dom_file_system) {
  private_ = dom_file_system;
  return *this;
}

}  // namespace blink
