/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/modules/filesystem/dom_window_file_system.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/modules/filesystem/choose_file_system_entries_options.h"
#include "third_party/blink/renderer/modules/filesystem/directory_entry.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_callbacks.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_dispatcher.h"
#include "third_party/blink/renderer/modules/filesystem/local_file_system.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

void DOMWindowFileSystem::webkitRequestFileSystem(
    LocalDOMWindow& window,
    int type,
    long long size,
    V8FileSystemCallback* success_callback,
    V8ErrorCallback* error_callback) {
  if (!window.IsCurrentlyDisplayedInFrame())
    return;

  Document* document = window.document();
  if (!document)
    return;

  if (SchemeRegistry::SchemeShouldBypassContentSecurityPolicy(
          document->GetSecurityOrigin()->Protocol()))
    UseCounter::Count(document, WebFeature::kRequestFileSystemNonWebbyOrigin);

  if (!document->GetSecurityOrigin()->CanAccessFileSystem()) {
    DOMFileSystem::ReportError(document,
                               ScriptErrorCallback::Wrap(error_callback),
                               base::File::FILE_ERROR_SECURITY);
    return;
  } else if (document->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(document, WebFeature::kFileAccessedFileSystem);
  }

  mojom::blink::FileSystemType file_system_type =
      static_cast<mojom::blink::FileSystemType>(type);
  if (!DOMFileSystemBase::IsValidType(file_system_type)) {
    DOMFileSystem::ReportError(document,
                               ScriptErrorCallback::Wrap(error_callback),
                               base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  LocalFileSystem::From(*document)->RequestFileSystem(
      document, file_system_type, size,
      FileSystemCallbacks::Create(
          FileSystemCallbacks::OnDidOpenFileSystemV8Impl::Create(
              success_callback),
          ScriptErrorCallback::Wrap(error_callback), document,
          file_system_type),
      LocalFileSystem::kAsynchronous);
}

void DOMWindowFileSystem::webkitResolveLocalFileSystemURL(
    LocalDOMWindow& window,
    const String& url,
    V8EntryCallback* success_callback,
    V8ErrorCallback* error_callback) {
  if (!window.IsCurrentlyDisplayedInFrame())
    return;

  Document* document = window.document();
  if (!document)
    return;

  const SecurityOrigin* security_origin = document->GetSecurityOrigin();
  KURL completed_url = document->CompleteURL(url);
  if (!security_origin->CanAccessFileSystem() ||
      !security_origin->CanRequest(completed_url)) {
    DOMFileSystem::ReportError(document,
                               ScriptErrorCallback::Wrap(error_callback),
                               base::File::FILE_ERROR_SECURITY);
    return;
  } else if (document->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(document, WebFeature::kFileAccessedFileSystem);
  }

  if (!completed_url.IsValid()) {
    DOMFileSystem::ReportError(document,
                               ScriptErrorCallback::Wrap(error_callback),
                               base::File::FILE_ERROR_INVALID_URL);
    return;
  }

  LocalFileSystem::From(*document)->ResolveURL(
      document, completed_url,
      ResolveURICallbacks::Create(
          ResolveURICallbacks::OnDidGetEntryV8Impl::Create(success_callback),
          ScriptErrorCallback::Wrap(error_callback), document),
      LocalFileSystem::kAsynchronous);
}

static_assert(
    static_cast<int>(DOMWindowFileSystem::kTemporary) ==
        static_cast<int>(mojom::blink::FileSystemType::kTemporary),
    "DOMWindowFileSystem::kTemporary should match FileSystemTypeTemporary");
static_assert(
    static_cast<int>(DOMWindowFileSystem::kPersistent) ==
        static_cast<int>(mojom::blink::FileSystemType::kPersistent),
    "DOMWindowFileSystem::kPersistent should match FileSystemTypePersistent");

namespace {

mojom::blink::ChooseFileSystemEntryType ConvertChooserType(const String& input,
                                                           bool multiple) {
  if (input == "openFile") {
    return multiple
               ? mojom::blink::ChooseFileSystemEntryType::kOpenMultipleFiles
               : mojom::blink::ChooseFileSystemEntryType::kOpenFile;
  }
  if (input == "saveFile")
    return mojom::blink::ChooseFileSystemEntryType::kSaveFile;
  if (input == "openDirectory")
    return mojom::blink::ChooseFileSystemEntryType::kOpenDirectory;
  NOTREACHED();
  return mojom::blink::ChooseFileSystemEntryType::kOpenFile;
}

Vector<mojom::blink::ChooseFileSystemEntryAcceptsOptionPtr> ConvertAccepts(
    const HeapVector<ChooseFileSystemEntriesOptionsAccepts>& accepts) {
  Vector<mojom::blink::ChooseFileSystemEntryAcceptsOptionPtr> result;
  result.ReserveInitialCapacity(accepts.size());
  for (const auto& a : accepts) {
    result.emplace_back(
        blink::mojom::blink::ChooseFileSystemEntryAcceptsOption::New(
            a.hasDescription() ? a.description() : g_empty_string,
            a.hasMimeTypes() ? a.mimeTypes() : Vector<String>(),
            a.hasExtensions() ? a.extensions() : Vector<String>()));
  }
  return result;
}

ScriptPromise CreateFileHandle(ScriptState* script_state,
                               const mojom::blink::FileSystemEntryPtr& entry,
                               bool is_directory) {
  auto* new_resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise result = new_resolver->Promise();
  auto* fs = DOMFileSystem::CreateIsolatedFileSystem(
      ExecutionContext::From(script_state), entry->file_system_id);
  // TODO(mek): Try to create handle directly rather than having to do more
  // IPCs to get the actual entries.
  if (is_directory) {
    fs->GetDirectory(fs->root(), entry->base_name, FileSystemFlags(),
                     new EntryCallbacks::OnDidGetEntryPromiseImpl(new_resolver),
                     new PromiseErrorCallback(new_resolver));
  } else {
    fs->GetFile(fs->root(), entry->base_name, FileSystemFlags(),
                new EntryCallbacks::OnDidGetEntryPromiseImpl(new_resolver),
                new PromiseErrorCallback(new_resolver));
  }
  return result;
}

}  // namespace

ScriptPromise DOMWindowFileSystem::chooseFileSystemEntries(
    ScriptState* script_state,
    LocalDOMWindow& window,
    const ChooseFileSystemEntriesOptions& options) {
  if (!base::FeatureList::IsEnabled(blink::features::kWritableFilesAPI)) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kAbortError));
  }

  if (!window.IsCurrentlyDisplayedInFrame()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kAbortError));
  }

  Document* document = window.document();
  if (!document) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kAbortError));
  }

  if (!LocalFrame::HasTransientUserActivation(window.GetFrame())) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            DOMExceptionCode::kSecurityError,
            "Must be handling a user gesture to show a file picker."));
  }

  Vector<mojom::blink::ChooseFileSystemEntryAcceptsOptionPtr> accepts;
  if (options.hasAccepts())
    accepts = ConvertAccepts(options.accepts());

  auto* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise result = resolver->Promise();
  FileSystemDispatcher::From(document).GetFileSystemManager().ChooseEntry(
      ConvertChooserType(options.type(), options.multiple()),
      std::move(accepts), !options.excludeAcceptAllOption(),
      WTF::Bind(
          [](ScriptPromiseResolver* resolver,
             const ChooseFileSystemEntriesOptions& options,
             base::File::Error result,
             Vector<mojom::blink::FileSystemEntryPtr> entries) {
            if (result != base::File::FILE_OK) {
              resolver->Reject(FileError::CreateDOMException(result));
              return;
            }
            bool is_directory = options.type() == "openDirectory";
            ScriptState* script_state = resolver->GetScriptState();
            ScriptState::Scope scope(script_state);
            if (options.multiple()) {
              Vector<ScriptPromise> result;
              result.ReserveInitialCapacity(entries.size());
              for (const auto& entry : entries) {
                result.emplace_back(
                    CreateFileHandle(script_state, entry, is_directory));
              }
              resolver->Resolve(
                  ScriptPromise::All(script_state, result).GetScriptValue());
            } else {
              DCHECK_EQ(1u, entries.size());
              resolver->Resolve(
                  CreateFileHandle(script_state, entries[0], is_directory)
                      .GetScriptValue());
            }
          },
          WrapPersistent(resolver), options));
  return result;
}

}  // namespace blink
