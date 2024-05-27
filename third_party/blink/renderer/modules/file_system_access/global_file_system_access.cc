// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/global_file_system_access.h"

#include <utility>

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_usvstring_usvstringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_directory_picker_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_picker_accept_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_open_file_picker_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_save_file_picker_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_filesystemhandle_wellknowndirectory.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_well_known_directory.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_error.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_manager.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_directory_handle.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_file_handle.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"

namespace blink {

namespace {

constexpr char kDefaultStartingDirectoryId[] = "";

constexpr bool IsHTTPWhitespace(UChar chr) {
  return chr == ' ' || chr == '\n' || chr == '\t' || chr == '\r';
}

bool IsValidSuffixCodePoint(UChar chr) {
  return IsASCIIAlphanumeric(chr) || chr == '+' || chr == '.';
}

bool IsValidIdCodePoint(UChar chr) {
  return IsASCIIAlphanumeric(chr) || chr == '_' || chr == '-';
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

String VerifyIsValidId(const String& id, ExceptionState& exception_state) {
  if (!id.IsAllSpecialCharacters<IsValidIdCodePoint>()) {
    exception_state.ThrowTypeError("ID '" + id +
                                   "' contains invalid characters.");
    return String();
  }
  if (id.length() > 32) {
    exception_state.ThrowTypeError("ID '" + id +
                                   "' cannot be longer than 32 characters.");
    return String();
  }

  return std::move(id);
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
    if (!t->hasAccept())
      continue;
    Vector<String> mimeTypes;
    mimeTypes.ReserveInitialCapacity(t->accept().size());
    Vector<String> extensions;
    for (const auto& a : t->accept()) {
      String type = a.first.StripWhiteSpace(IsHTTPWhitespace);
      if (type.empty()) {
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
      switch (a.second->GetContentType()) {
        case V8UnionUSVStringOrUSVStringSequence::ContentType::kUSVString:
          if (!AddExtension(a.second->GetAsUSVString(), extensions,
                            exception_state)) {
            return {};
          }
          break;
        case V8UnionUSVStringOrUSVStringSequence::ContentType::
            kUSVStringSequence:
          for (const auto& extension : a.second->GetAsUSVStringSequence()) {
            if (!AddExtension(extension, extensions, exception_state)) {
              return {};
            }
          }
          break;
      }
    }
    result.emplace_back(
        blink::mojom::blink::ChooseFileSystemEntryAcceptsOption::New(
            t->description(), std::move(mimeTypes), std::move(extensions)));
  }
  return result;
}

void VerifyIsAllowedToShowFilePicker(const LocalDOMWindow& window,
                                     ExceptionState& exception_state) {
  if (!window.IsCurrentlyDisplayedInFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError, "");
    return;
  }

  if (!window.GetSecurityOrigin()->CanAccessFileSystem()) {
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
  if (!local_frame || local_frame->IsCrossOriginToOutermostMainFrame()) {
    exception_state.ThrowSecurityError(
        "Cross origin sub frames aren't allowed to show a file picker.");
    return;
  }

  if (!LocalFrame::HasTransientUserActivation(local_frame) &&
      local_frame->GetSettings()
          ->GetRequireTransientActivationForShowFileOrDirectoryPicker()) {
    exception_state.ThrowSecurityError(
        "Must be handling a user gesture to show a file picker.");
    return;
  }
}

mojom::blink::WellKnownDirectory ToMojomWellKnownDirectory(
    V8WellKnownDirectory v8_well_known_directory) {
  // This assertion protects against the IDL enum changing without updating the
  // corresponding mojom interface, or vice versa. The offset of 1 accounts for
  // the zero-indexing of the mojom enum values.
  static_assert(
      V8WellKnownDirectory::kEnumSize ==
          static_cast<size_t>(mojom::blink::WellKnownDirectory::kMaxValue) + 1,
      "the number of values in the WellKnownDirectory mojom enum "
      "must match the number of values in the WellKnownDirectory blink enum");

  switch (v8_well_known_directory.AsEnum()) {
    case V8WellKnownDirectory::Enum::kDesktop:
      return mojom::blink::WellKnownDirectory::kDirDesktop;
    case V8WellKnownDirectory::Enum::kDocuments:
      return mojom::blink::WellKnownDirectory::kDirDocuments;
    case V8WellKnownDirectory::Enum::kDownloads:
      return mojom::blink::WellKnownDirectory::kDirDownloads;
    case V8WellKnownDirectory::Enum::kMusic:
      return mojom::blink::WellKnownDirectory::kDirMusic;
    case V8WellKnownDirectory::Enum::kPictures:
      return mojom::blink::WellKnownDirectory::kDirPictures;
    case V8WellKnownDirectory::Enum::kVideos:
      return mojom::blink::WellKnownDirectory::kDirVideos;
  }
}

mojom::blink::FilePickerStartInOptionsUnionPtr ToMojomStartInOptions(
    const V8UnionFileSystemHandleOrWellKnownDirectory* start_in_union) {
  switch (start_in_union->GetContentType()) {
    case V8UnionFileSystemHandleOrWellKnownDirectory::ContentType::
        kFileSystemHandle:
      return mojom::blink::FilePickerStartInOptionsUnion::NewDirectoryToken(
          start_in_union->GetAsFileSystemHandle()->Transfer());
    case V8UnionFileSystemHandleOrWellKnownDirectory::ContentType::
        kWellKnownDirectory:
      return mojom::blink::FilePickerStartInOptionsUnion::NewWellKnownDirectory(
          ToMojomWellKnownDirectory(start_in_union->GetAsWellKnownDirectory()));
  }
}

enum class ShowFilePickerType { kSequence, kHandle, kDirectory };

void ShowFilePickerImpl(ScriptPromiseResolverBase* resolver,
                        LocalDOMWindow& window,
                        mojom::blink::FilePickerOptionsPtr options,
                        ExceptionState& exception_state,
                        ShowFilePickerType type) {
  bool multiple =
      options->type_specific_options->is_open_file_picker_options() &&
      options->type_specific_options->get_open_file_picker_options()
          ->can_select_multiple_files;
  bool intercepted = false;
  probe::FileChooserOpened(window.GetFrame(), /*element=*/nullptr, multiple,
                           &intercepted);
  if (intercepted) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kAbortError,
        "Intercepted by Page.setInterceptFileChooserDialog().");
    return;
  }

  FileSystemAccessManager::From(resolver->GetExecutionContext())
      ->ChooseEntries(
          std::move(options),
          WTF::BindOnce(
              [](ScriptPromiseResolverBase* resolver, ShowFilePickerType type,
                 LocalFrame* local_frame,
                 mojom::blink::FileSystemAccessErrorPtr file_operation_result,
                 Vector<mojom::blink::FileSystemAccessEntryPtr> entries) {
                ExecutionContext* context = resolver->GetExecutionContext();
                if (!context) {
                  return;
                }
                if (file_operation_result->status !=
                    mojom::blink::FileSystemAccessStatus::kOk) {
                  file_system_access_error::Reject(resolver,
                                                   *file_operation_result);
                  return;
                }

                // While it would be better to not trust the renderer process,
                // we're doing this here to avoid potential mojo message pipe
                // ordering problems, where the frame activation state
                // reconciliation messages would compete with concurrent File
                // System Access messages to the browser.
                // TODO(https://crbug.com/1017270): Remove this after spec
                // change, or when activation moves to browser.
                LocalFrame::NotifyUserActivation(
                    local_frame, mojom::blink::UserActivationNotificationType::
                                     kFileSystemAccess);

                if (type == ShowFilePickerType::kSequence) {
                  HeapVector<Member<FileSystemFileHandle>> results;
                  results.ReserveInitialCapacity(entries.size());
                  for (auto& entry : entries) {
                    auto* handle = FileSystemHandle::CreateFromMojoEntry(
                        std::move(entry), context);
                    results.push_back(To<FileSystemFileHandle>(handle));
                  }
                  resolver->DowncastTo<IDLSequence<FileSystemFileHandle>>()
                      ->Resolve(results);
                } else {
                  DCHECK_EQ(1u, entries.size());
                  auto* handle = FileSystemHandle::CreateFromMojoEntry(
                      std::move(entries[0]), context);
                  if (type == ShowFilePickerType::kHandle) {
                    resolver->DowncastTo<FileSystemFileHandle>()->Resolve(
                        To<FileSystemFileHandle>(handle));
                  } else {
                    resolver->DowncastTo<FileSystemDirectoryHandle>()->Resolve(
                        To<FileSystemDirectoryHandle>(handle));
                  }
                }
              },
              WrapPersistent(resolver), type,
              WrapPersistent(window.GetFrame())));
}

}  // namespace

// static
ScriptPromise<IDLSequence<FileSystemFileHandle>>
GlobalFileSystemAccess::showOpenFilePicker(ScriptState* script_state,
                                           LocalDOMWindow& window,
                                           const OpenFilePickerOptions* options,
                                           ExceptionState& exception_state) {
  UseCounter::Count(window, WebFeature::kFileSystemPickerMethod);

  Vector<mojom::blink::ChooseFileSystemEntryAcceptsOptionPtr> accepts;
  if (options->hasTypes())
    accepts = ConvertAccepts(options->types(), exception_state);
  if (exception_state.HadException())
    return ScriptPromise<IDLSequence<FileSystemFileHandle>>();

  if (accepts.empty() && options->excludeAcceptAllOption()) {
    exception_state.ThrowTypeError("Need at least one accepted type");
    return ScriptPromise<IDLSequence<FileSystemFileHandle>>();
  }

  String starting_directory_id = kDefaultStartingDirectoryId;
  if (options->hasId()) {
    starting_directory_id = VerifyIsValidId(options->id(), exception_state);
    if (exception_state.HadException())
      return ScriptPromise<IDLSequence<FileSystemFileHandle>>();
  }

  mojom::blink::FilePickerStartInOptionsUnionPtr start_in_options;
  if (options->hasStartIn()) {
    start_in_options = ToMojomStartInOptions(options->startIn());
  }

  VerifyIsAllowedToShowFilePicker(window, exception_state);
  if (exception_state.HadException())
    return ScriptPromise<IDLSequence<FileSystemFileHandle>>();

  auto open_file_picker_options = mojom::blink::OpenFilePickerOptions::New(
      mojom::blink::AcceptsTypesInfo::New(std::move(accepts),
                                          !options->excludeAcceptAllOption()),
      options->multiple());

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLSequence<FileSystemFileHandle>>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  ShowFilePickerImpl(
      resolver, window,
      mojom::blink::FilePickerOptions::New(
          mojom::blink::TypeSpecificFilePickerOptionsUnion::
              NewOpenFilePickerOptions(std::move(open_file_picker_options)),
          std::move(starting_directory_id), std::move(start_in_options)),
      exception_state, ShowFilePickerType::kSequence);
  return promise;
}

// static
ScriptPromise<FileSystemFileHandle> GlobalFileSystemAccess::showSaveFilePicker(
    ScriptState* script_state,
    LocalDOMWindow& window,
    const SaveFilePickerOptions* options,
    ExceptionState& exception_state) {
  UseCounter::Count(window, WebFeature::kFileSystemPickerMethod);

  Vector<mojom::blink::ChooseFileSystemEntryAcceptsOptionPtr> accepts;
  if (options->hasTypes())
    accepts = ConvertAccepts(options->types(), exception_state);
  if (exception_state.HadException())
    return EmptyPromise();

  if (accepts.empty() && options->excludeAcceptAllOption()) {
    exception_state.ThrowTypeError("Need at least one accepted type");
    return EmptyPromise();
  }

  String starting_directory_id = kDefaultStartingDirectoryId;
  if (options->hasId()) {
    starting_directory_id = VerifyIsValidId(options->id(), exception_state);
    if (exception_state.HadException())
      return EmptyPromise();
  }

  mojom::blink::FilePickerStartInOptionsUnionPtr start_in_options;
  if (options->hasStartIn()) {
    start_in_options = ToMojomStartInOptions(options->startIn());
  }

  VerifyIsAllowedToShowFilePicker(window, exception_state);
  if (exception_state.HadException())
    return EmptyPromise();

  auto save_file_picker_options = mojom::blink::SaveFilePickerOptions::New(
      mojom::blink::AcceptsTypesInfo::New(std::move(accepts),
                                          !options->excludeAcceptAllOption()),
      (options->hasSuggestedName() && !options->suggestedName().IsNull())
          ? options->suggestedName()
          : g_empty_string);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<FileSystemFileHandle>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  ShowFilePickerImpl(
      resolver, window,
      mojom::blink::FilePickerOptions::New(
          mojom::blink::TypeSpecificFilePickerOptionsUnion::
              NewSaveFilePickerOptions(std::move(save_file_picker_options)),
          std::move(starting_directory_id), std::move(start_in_options)),
      exception_state, ShowFilePickerType::kHandle);
  return promise;
}

// static
ScriptPromise<FileSystemDirectoryHandle>
GlobalFileSystemAccess::showDirectoryPicker(
    ScriptState* script_state,
    LocalDOMWindow& window,
    const DirectoryPickerOptions* options,
    ExceptionState& exception_state) {
  UseCounter::Count(window, WebFeature::kFileSystemPickerMethod);

  String starting_directory_id = kDefaultStartingDirectoryId;
  if (options->hasId()) {
    starting_directory_id = VerifyIsValidId(options->id(), exception_state);
    if (exception_state.HadException())
      return EmptyPromise();
  }

  mojom::blink::FilePickerStartInOptionsUnionPtr start_in_options;
  if (options->hasStartIn()) {
    start_in_options = ToMojomStartInOptions(options->startIn());
  }

  VerifyIsAllowedToShowFilePicker(window, exception_state);
  if (exception_state.HadException())
    return EmptyPromise();

  bool request_writable =
      options->mode() == V8FileSystemPermissionMode::Enum::kReadwrite;
  auto directory_picker_options =
      mojom::blink::DirectoryPickerOptions::New(request_writable);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<FileSystemDirectoryHandle>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  ShowFilePickerImpl(
      resolver, window,
      mojom::blink::FilePickerOptions::New(
          mojom::blink::TypeSpecificFilePickerOptionsUnion::
              NewDirectoryPickerOptions(std::move(directory_picker_options)),
          std::move(starting_directory_id), std::move(start_in_options)),
      exception_state, ShowFilePickerType::kDirectory);
  return promise;
}

}  // namespace blink
