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
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
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

WebDOMFileSystem WebDOMFileSystem::FromV8Value(v8::Isolate* isolate,
                                               v8::Local<v8::Value> value) {
  if (DOMFileSystem* dom_file_system =
          V8DOMFileSystem::ToWrappable(isolate, value)) {
    return WebDOMFileSystem(dom_file_system);
  }
  return WebDOMFileSystem();
}

WebURL WebDOMFileSystem::CreateFileSystemURL(v8::Isolate* isolate,
                                             v8::Local<v8::Value> value) {
  const Entry* const entry = V8Entry::ToWrappable(isolate, value);
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
      NOTREACHED_IN_MIGRATION();
      return WebFileSystemType::kWebFileSystemTypeTemporary;
  }
}

WebURL WebDOMFileSystem::RootURL() const {
  DCHECK(private_.Get());
  return private_->RootURL();
}

v8::Local<v8::Value> WebDOMFileSystem::ToV8Value(v8::Isolate* isolate) {
  if (!private_.Get())
    return v8::Local<v8::Value>();
  return ToV8Traits<DOMFileSystem>::ToV8(ScriptState::ForCurrentRealm(isolate),
                                         private_.Get());
}

v8::Local<v8::Value> WebDOMFileSystem::CreateV8Entry(
    const WebString& path,
    EntryType entry_type,
    v8::Isolate* isolate) {
  if (!private_.Get())
    return v8::Local<v8::Value>();
  v8::Local<v8::Value> value;
  ScriptState* script_state = ScriptState::ForCurrentRealm(isolate);
  switch (entry_type) {
    case kEntryTypeDirectory:
      value = ToV8Traits<DirectoryEntry>::ToV8(
          script_state,
          MakeGarbageCollected<DirectoryEntry>(private_.Get(), path));
      break;
    case kEntryTypeFile:
      value = ToV8Traits<FileEntry>::ToV8(
          script_state, MakeGarbageCollected<FileEntry>(private_.Get(), path));
      break;
  }
  return value;
}

WebDOMFileSystem::WebDOMFileSystem(DOMFileSystem* dom_file_system)
    : private_(dom_file_system) {}

WebDOMFileSystem& WebDOMFileSystem::operator=(DOMFileSystem* dom_file_system) {
  private_ = dom_file_system;
  return *this;
}

}  // namespace blink
