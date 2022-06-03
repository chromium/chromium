/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "third_party/blink/renderer/modules/filesystem/entry.h"

#include "third_party/blink/public/mojom/filesystem/file_system.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/filesystem/async_callback_helper.h"
#include "third_party/blink/renderer/modules/filesystem/directory_entry.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_callbacks.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

Entry::Entry(DOMFileSystemBase* file_system, const String& full_path)
    : EntryBase(file_system, full_path) {}

DOMFileSystem* Entry::filesystem(ScriptState* script_state) const {
  if (file_system_->GetType() == mojom::blink::FileSystemType::kIsolated) {
    UseCounter::Count(
        ExecutionContext::From(script_state),
        WebFeature::kEntry_Filesystem_AttributeGetter_IsolatedFileSystem);
  }
  return filesystem();
}

void Entry::getMetadata(ScriptState* script_state,
                        V8MetadataCallback* success_callback,
                        V8ErrorCallback* error_callback) {
  if (file_system_->GetType() == mojom::blink::FileSystemType::kIsolated) {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kEntry_GetMetadata_Method_IsolatedFileSystem);
  }

  auto success_callback_wrapper =
      AsyncCallbackHelper::SuccessCallback<Metadata>(success_callback);
  auto error_callback_wrapper =
      AsyncCallbackHelper::ErrorCallback(error_callback);

  file_system_->GetMetadata(this, std::move(success_callback_wrapper),
                            std::move(error_callback_wrapper));
}

void Entry::moveTo(ScriptState* script_state,
                   DirectoryEntry* parent,
                   const String& name,
                   V8EntryCallback* success_callback,
                   V8ErrorCallback* error_callback) const {
  if (file_system_->GetType() == mojom::blink::FileSystemType::kIsolated) {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kEntry_MoveTo_Method_IsolatedFileSystem);
  }

  auto success_callback_wrapper =
      AsyncCallbackHelper::SuccessCallback<Entry>(success_callback);
  auto error_callback_wrapper =
      AsyncCallbackHelper::ErrorCallback(error_callback);

  file_system_->Move(this, parent, name, std::move(success_callback_wrapper),
                     std::move(error_callback_wrapper));
}

void Entry::copyTo(ScriptState* script_state,
                   DirectoryEntry* parent,
                   const String& name,
                   V8EntryCallback* success_callback,
                   V8ErrorCallback* error_callback) const {
  if (file_system_->GetType() == mojom::blink::FileSystemType::kIsolated) {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kEntry_CopyTo_Method_IsolatedFileSystem);
  }

  auto success_callback_wrapper =
      AsyncCallbackHelper::SuccessCallback<Entry>(success_callback);
  auto error_callback_wrapper =
      AsyncCallbackHelper::ErrorCallback(error_callback);

  file_system_->Copy(this, parent, name, std::move(success_callback_wrapper),
                     std::move(error_callback_wrapper));
}

void Entry::remove(ScriptState* script_state,
                   V8VoidCallback* success_callback,
                   V8ErrorCallback* error_callback) const {
  if (file_system_->GetType() == mojom::blink::FileSystemType::kIsolated) {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kEntry_Remove_Method_IsolatedFileSystem);
  }

  auto success_callback_wrapper =
      AsyncCallbackHelper::VoidSuccessCallback(success_callback);
  auto error_callback_wrapper =
      AsyncCallbackHelper::ErrorCallback(error_callback);

  file_system_->Remove(this, std::move(success_callback_wrapper),
                       std::move(error_callback_wrapper));
}

void Entry::getParent(ScriptState* script_state,
                      V8EntryCallback* success_callback,
                      V8ErrorCallback* error_callback) const {
  if (file_system_->GetType() == mojom::blink::FileSystemType::kIsolated) {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kEntry_GetParent_Method_IsolatedFileSystem);
  }
  auto success_callback_wrapper =
      AsyncCallbackHelper::SuccessCallback<Entry>(success_callback);
  auto error_callback_wrapper =
      AsyncCallbackHelper::ErrorCallback(error_callback);

  file_system_->GetParent(this, std::move(success_callback_wrapper),
                          std::move(error_callback_wrapper));
}

String Entry::toURL(ScriptState* script_state) const {
  if (file_system_->GetType() == mojom::blink::FileSystemType::kIsolated) {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kEntry_ToURL_Method_IsolatedFileSystem);
  }
  return static_cast<const EntryBase*>(this)->toURL();
}

void Entry::Trace(Visitor* visitor) const {
  EntryBase::Trace(visitor);
}

}  // namespace blink
