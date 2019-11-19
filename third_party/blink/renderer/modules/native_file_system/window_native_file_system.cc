// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_file_system/window_native_file_system.h"

#include <utility>

#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/native_file_system/choose_file_system_entries_options.h"
#include "third_party/blink/renderer/modules/native_file_system/choose_file_system_entries_options_accepts.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_directory_handle.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_error.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_file_handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

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
    const HeapVector<Member<ChooseFileSystemEntriesOptionsAccepts>>& accepts) {
  Vector<mojom::blink::ChooseFileSystemEntryAcceptsOptionPtr> result;
  result.ReserveInitialCapacity(accepts.size());
  for (const auto& a : accepts) {
    result.emplace_back(
        blink::mojom::blink::ChooseFileSystemEntryAcceptsOption::New(
            a->hasDescription() ? a->description() : g_empty_string,
            a->hasMimeTypes() ? a->mimeTypes() : Vector<String>(),
            a->hasExtensions() ? a->extensions() : Vector<String>()));
  }
  return result;
}

}  // namespace

// static
ScriptPromise WindowNativeFileSystem::chooseFileSystemEntries(
    ScriptState* script_state,
    LocalDOMWindow& window,
    const ChooseFileSystemEntriesOptions* options) {
  if (!window.IsCurrentlyDisplayedInFrame()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError));
  }

  Document* document = window.document();
  if (!document) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError));
  }

  if (!document->GetSecurityOrigin()->CanAccessNativeFileSystem()) {
    if (document->IsSandboxed(WebSandboxFlags::kOrigin)) {
      return ScriptPromise::RejectWithDOMException(
          script_state,
          MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kSecurityError,
              "Sandboxed documents aren't allowed to show a file picker."));
    } else {
      return ScriptPromise::RejectWithDOMException(
          script_state,
          MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kSecurityError,
              "This document isn't allowed to show a file picker."));
    }
  }

  LocalFrame* local_frame = window.GetFrame();
  if (!local_frame || local_frame->IsCrossOriginSubframe()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kSecurityError,
            "Cross origin sub frames aren't allowed to show a file picker."));
  }

  if (!LocalFrame::HasTransientUserActivation(local_frame)) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kSecurityError,
            "Must be handling a user gesture to show a file picker."));
  }

  Vector<mojom::blink::ChooseFileSystemEntryAcceptsOptionPtr> accepts;
  if (options->hasAccepts())
    accepts = ConvertAccepts(options->accepts());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise resolver_result = resolver->Promise();

  // TODO(mek): Cache mojo::Remote<mojom::blink::NativeFileSystemManager>
  // associated with an ExecutionContext, so we don't have to request a new one
  // for each operation, and can avoid code duplication between here and other
  // uses.
  mojo::Remote<mojom::blink::NativeFileSystemManager> manager;
  auto* provider = document->GetInterfaceProvider();
  if (!provider) {
    resolver->Reject(file_error::CreateDOMException(
        base::File::FILE_ERROR_INVALID_OPERATION));
    return resolver_result;
  }

  provider->GetInterface(manager.BindNewPipeAndPassReceiver());
  auto* raw_manager = manager.get();
  raw_manager->ChooseEntries(
      ConvertChooserType(options->type(), options->multiple()),
      std::move(accepts), !options->excludeAcceptAllOption(),
      WTF::Bind(
          [](ScriptPromiseResolver* resolver,
             mojo::Remote<mojom::blink::NativeFileSystemManager>,
             const ChooseFileSystemEntriesOptions* options,
             LocalFrame* local_frame,
             mojom::blink::NativeFileSystemErrorPtr file_operation_result,
             Vector<mojom::blink::NativeFileSystemEntryPtr> entries) {
            ExecutionContext* context = resolver->GetExecutionContext();
            if (!context)
              return;
            if (file_operation_result->status !=
                mojom::blink::NativeFileSystemStatus::kOk) {
              native_file_system_error::Reject(resolver,
                                               *file_operation_result);
              return;
            }

            // While it would be better to not trust the renderer process,
            // we're doing this here to avoid potential mojo message pipe
            // ordering problems, where the frame activation state
            // reconciliation messages would compete with concurrent Native File
            // System messages to the browser.
            // TODO(https://crbug.com/1017270): Remove this after spec change,
            // or when activation moves to browser.
            LocalFrame::NotifyUserActivation(local_frame);

            if (options->multiple()) {
              HeapVector<Member<NativeFileSystemHandle>> results;
              results.ReserveInitialCapacity(entries.size());
              for (auto& entry : entries) {
                results.push_back(NativeFileSystemHandle::CreateFromMojoEntry(
                    std::move(entry), context));
              }
              resolver->Resolve(results);
            } else {
              DCHECK_EQ(1u, entries.size());
              resolver->Resolve(NativeFileSystemHandle::CreateFromMojoEntry(
                  std::move(entries[0]), context));
            }
          },
          WrapPersistent(resolver), std::move(manager), WrapPersistent(options),
          WrapPersistent(local_frame)));
  return resolver_result;
}

}  // namespace blink
