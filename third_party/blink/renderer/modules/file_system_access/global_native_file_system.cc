// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/global_native_file_system.h"

#include <utility>

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_directory_picker_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_picker_accept_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_open_file_picker_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_save_file_picker_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/file_system_access/native_file_system_directory_handle.h"
#include "third_party/blink/renderer/modules/file_system_access/native_file_system_error.h"
#include "third_party/blink/renderer/modules/file_system_access/native_file_system_file_handle.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"

namespace blink {

namespace {

constexpr bool IsHTTPWhitespace(UChar chr) {
  return chr == ' ' || chr == '\n' || chr == '\t' || chr == '\r';
}

bool IsValidSuffixCodePoint(UChar chr) {
  return IsASCIIAlphanumeric(chr) || chr == '+' || chr == '.';
}

bool VerifyIsValidExtension(const String& extension,
                            ExceptionState& exception_state) {
  if (!extension.StartsWith(".")) {
    exception_state.ThrowTypeError("Extension '" + extension +
                                   "' must start with '.'.");
    return false;
  }
  if (!extension.IsAllSpecialCharacters<IsValidSuffixCodePoint>()) {
    exception_state.ThrowTypeError("Extension '" + extension +
                                   "' contains invalid characters.");
    return false;
  }
  if (extension.EndsWith(".")) {
    exception_state.ThrowTypeError("Extension '" + extension +
                                   "' must not end with '.'.");
    return false;
  }
  if (extension.length() > 16) {
    exception_state.ThrowTypeError("Extension '" + extension +
                                   "' cannot be longer than 16 characters.");
    return false;
  }

  return true;
}

bool AddExtension(const String& extension,
                  Vector<String>& extensions,
                  ExceptionState& exception_state) {
  if (!VerifyIsValidExtension(extension, exception_state))
    return false;

  extensions.push_back(extension.Substring(1));
  return true;
}

Vector<mojom::blink::ChooseFileSystemEntryAcceptsOptionPtr> ConvertAccepts(
    const HeapVector<Member<FilePickerAcceptType>>& types,
    ExceptionState& exception_state) {
  Vector<mojom::blink::ChooseFileSystemEntryAcceptsOptionPtr> result;
  result.ReserveInitialCapacity(types.size());
  for (const auto& t : types) {
    Vector<String> mimeTypes;
    mimeTypes.ReserveInitialCapacity(t->accept().size());
    Vector<String> extensions;
    for (const auto& a : t->accept()) {
      String type = a.first.StripWhiteSpace(IsHTTPWhitespace);
      if (type.IsEmpty()) {
        exception_state.ThrowTypeError("Invalid type: " + a.first);
        return {};
      }
      Vector<String> parsed_type;
      type.Split('/', true, parsed_type);
      if (parsed_type.size() != 2) {
        exception_state.ThrowTypeError("Invalid type: " + a.first);
        return {};
      }
      if (!IsValidHTTPToken(parsed_type[0])) {
        exception_state.ThrowTypeError("Invalid type: " + a.first);
        return {};
      }
      if (!IsValidHTTPToken(parsed_type[1])) {
        exception_state.ThrowTypeError("Invalid type: " + a.first);
        return {};
      }

      mimeTypes.push_back(type);
      if (a.second.IsUSVString()) {
        if (!AddExtension(a.second.GetAsUSVString(), extensions,
                          exception_state))
          return {};
      } else {
        for (const auto& extension : a.second.GetAsUSVStringSequence()) {
          if (!AddExtension(extension, extensions, exception_state))
            return {};
        }
      }
    }
    result.emplace_back(
        blink::mojom::blink::ChooseFileSystemEntryAcceptsOption::New(
            t->hasDescription() ? t->description() : g_empty_string,
            std::move(mimeTypes), std::move(extensions)));
  }
  return result;
}

void VerifyIsAllowedToShowFilePicker(const LocalDOMWindow& window,
                                     ExceptionState& exception_state) {
  if (!window.IsCurrentlyDisplayedInFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError, "");
    return;
  }

  if (!window.GetSecurityOrigin()->CanAccessNativeFileSystem()) {
    if (window.IsSandboxed(network::mojom::blink::WebSandboxFlags::kOrigin)) {
      exception_state.ThrowSecurityError(
          "Sandboxed documents aren't allowed to show a file picker.");
      return;
    } else {
      exception_state.ThrowSecurityError(
          "This document isn't allowed to show a file picker.");
      return;
    }
  }

  LocalFrame* local_frame = window.GetFrame();
  if (!local_frame || local_frame->IsCrossOriginToMainFrame()) {
    exception_state.ThrowSecurityError(
        "Cross origin sub frames aren't allowed to show a file picker.");
    return;
  }

  if (!LocalFrame::HasTransientUserActivation(local_frame)) {
    exception_state.ThrowSecurityError(
        "Must be handling a user gesture to show a file picker.");
    return;
  }
}

mojom::blink::CommonDirectory ConvertCommonDirectory(
    const String& starting_directory,
    ExceptionState& exception_state) {
  if (starting_directory == "")
    return mojom::blink::CommonDirectory::kDefault;
  else if (starting_directory == "desktop")
    return mojom::blink::CommonDirectory::kDirDesktop;
  else if (starting_directory == "documents")
    return mojom::blink::CommonDirectory::kDirDocuments;
  else if (starting_directory == "downloads")
    return mojom::blink::CommonDirectory::kDirDownloads;
  else if (starting_directory == "home")
    return mojom::blink::CommonDirectory::kDirHome;
  else if (starting_directory == "music")
    return mojom::blink::CommonDirectory::kDirMusic;
  else if (starting_directory == "pictures")
    return mojom::blink::CommonDirectory::kDirPictures;
  else if (starting_directory == "videos")
    return mojom::blink::CommonDirectory::kDirVideos;

  NOTREACHED();
  return mojom::blink::CommonDirectory::kDefault;
}

ScriptPromise ShowFilePickerImpl(
    ScriptState* script_state,
    LocalDOMWindow& window,
    mojom::blink::ChooseFileSystemEntryType chooser_type,
    Vector<mojom::blink::ChooseFileSystemEntryAcceptsOptionPtr> accepts,
    mojom::blink::CommonDirectory starting_directory,
    bool accept_all,
    bool return_as_sequence) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise resolver_result = resolver->Promise();

  // TODO(mek): Cache mojo::Remote<mojom::blink::FileSystemAccessManager>
  // associated with an ExecutionContext, so we don't have to request a new one
  // for each operation, and can avoid code duplication between here and other
  // uses.
  mojo::Remote<mojom::blink::FileSystemAccessManager> manager;
  window.GetBrowserInterfaceBroker().GetInterface(
      manager.BindNewPipeAndPassReceiver());

  auto* raw_manager = manager.get();
  raw_manager->ChooseEntries(
      chooser_type, std::move(accepts),
      RuntimeEnabledFeatures::FileSystemAccessAPIExperimentalEnabled()
          ? starting_directory
          : mojom::blink::CommonDirectory::kDefault,
      accept_all,
      WTF::Bind(
          [](ScriptPromiseResolver* resolver,
             mojo::Remote<mojom::blink::FileSystemAccessManager>,
             bool return_as_sequence, LocalFrame* local_frame,
             mojom::blink::FileSystemAccessErrorPtr file_operation_result,
             Vector<mojom::blink::FileSystemAccessEntryPtr> entries) {
            ExecutionContext* context = resolver->GetExecutionContext();
            if (!context)
              return;
            if (file_operation_result->status !=
                mojom::blink::FileSystemAccessStatus::kOk) {
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
            LocalFrame::NotifyUserActivation(
                local_frame, mojom::blink::UserActivationNotificationType::
                                 kNativeFileSystem);

            if (return_as_sequence) {
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
          WrapPersistent(resolver), std::move(manager), return_as_sequence,
          WrapPersistent(window.GetFrame())));
  return resolver_result;
}

}  // namespace

// static
ScriptPromise GlobalNativeFileSystem::showOpenFilePicker(
    ScriptState* script_state,
    LocalDOMWindow& window,
    const OpenFilePickerOptions* options,
    ExceptionState& exception_state) {
  UseCounter::Count(window, WebFeature::kFileSystemPickerMethod);

  Vector<mojom::blink::ChooseFileSystemEntryAcceptsOptionPtr> accepts;
  if (options->hasTypes())
    accepts = ConvertAccepts(options->types(), exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  if (accepts.IsEmpty() && options->excludeAcceptAllOption()) {
    exception_state.ThrowTypeError("Need at least one accepted type");
    return ScriptPromise();
  }

  auto starting_directory = mojom::blink::CommonDirectory::kDefault;
  if (options->hasStartInNonNull()) {
    starting_directory = ConvertCommonDirectory(
        IDLEnumAsString(options->startInNonNull()), exception_state);
    if (exception_state.HadException())
      return ScriptPromise();
  }

  VerifyIsAllowedToShowFilePicker(window, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  return ShowFilePickerImpl(
      script_state, window,
      options->multiple()
          ? mojom::blink::ChooseFileSystemEntryType::kOpenMultipleFiles
          : mojom::blink::ChooseFileSystemEntryType::kOpenFile,
      std::move(accepts), starting_directory,
      !options->excludeAcceptAllOption(),
      /*return_as_sequence=*/true);
}

// static
ScriptPromise GlobalNativeFileSystem::showSaveFilePicker(
    ScriptState* script_state,
    LocalDOMWindow& window,
    const SaveFilePickerOptions* options,
    ExceptionState& exception_state) {
  UseCounter::Count(window, WebFeature::kFileSystemPickerMethod);

  Vector<mojom::blink::ChooseFileSystemEntryAcceptsOptionPtr> accepts;
  if (options->hasTypes())
    accepts = ConvertAccepts(options->types(), exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  if (accepts.IsEmpty() && options->excludeAcceptAllOption()) {
    exception_state.ThrowTypeError("Need at least one accepted type");
    return ScriptPromise();
  }

  auto starting_directory = mojom::blink::CommonDirectory::kDefault;
  if (options->hasStartInNonNull()) {
    starting_directory = ConvertCommonDirectory(
        IDLEnumAsString(options->startInNonNull()), exception_state);
    if (exception_state.HadException())
      return ScriptPromise();
  }

  VerifyIsAllowedToShowFilePicker(window, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  return ShowFilePickerImpl(script_state, window,
                            mojom::blink::ChooseFileSystemEntryType::kSaveFile,
                            std::move(accepts), starting_directory,
                            !options->excludeAcceptAllOption(),
                            /*return_as_sequence=*/false);
}

// static
ScriptPromise GlobalNativeFileSystem::showDirectoryPicker(
    ScriptState* script_state,
    LocalDOMWindow& window,
    const DirectoryPickerOptions* options,
    ExceptionState& exception_state) {
  UseCounter::Count(window, WebFeature::kFileSystemPickerMethod);

  auto starting_directory = mojom::blink::CommonDirectory::kDefault;
  if (options->hasStartInNonNull()) {
    starting_directory = ConvertCommonDirectory(
        IDLEnumAsString(options->startInNonNull()), exception_state);
    if (exception_state.HadException())
      return ScriptPromise();
  }
  VerifyIsAllowedToShowFilePicker(window, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  return ShowFilePickerImpl(
      script_state, window,
      mojom::blink::ChooseFileSystemEntryType::kOpenDirectory, {},
      starting_directory,
      /*accept_all=*/true,
      /*return_as_sequence=*/false);
}

}  // namespace blink
