// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_handle.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_cloud_identifier.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_cloud_identifier.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_handle_permission_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_permission_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_permission_state.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_error.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_directory_handle.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_file_handle.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
using mojom::blink::FileSystemAccessEntryPtr;
using mojom::blink::FileSystemAccessErrorPtr;

FileSystemHandle::FileSystemHandle(ExecutionContext* execution_context,
                                   const String& name)
    : ExecutionContextClient(execution_context), name_(name) {}

// static
FileSystemHandle* FileSystemHandle::CreateFromMojoEntry(
    mojom::blink::FileSystemAccessEntryPtr e,
    ExecutionContext* execution_context) {
  if (e->entry_handle->is_file()) {
    return MakeGarbageCollected<FileSystemFileHandle>(
        execution_context, e->name, std::move(e->entry_handle->get_file()));
  }
  return MakeGarbageCollected<FileSystemDirectoryHandle>(
      execution_context, e->name, std::move(e->entry_handle->get_directory()));
}

ScriptPromise<V8PermissionState> FileSystemHandle::queryPermission(
    ScriptState* script_state,
    const FileSystemHandlePermissionDescriptor* descriptor) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8PermissionState>>(
          script_state);
  auto result = resolver->Promise();

  QueryPermissionImpl(
      descriptor->mode() == V8FileSystemPermissionMode::Enum::kReadwrite,
      WTF::BindOnce(
          [](FileSystemHandle* handle,
             ScriptPromiseResolver<V8PermissionState>* resolver,
             mojom::blink::PermissionStatus result) {
            // Keep `this` alive so the handle will not be garbage-collected
            // before the promise is resolved.
            resolver->Resolve(ToV8PermissionState(result));
          },
          WrapPersistent(this), WrapPersistent(resolver)));

  return result;
}

ScriptPromise<V8PermissionState> FileSystemHandle::requestPermission(
    ScriptState* script_state,
    const FileSystemHandlePermissionDescriptor* descriptor,
    ExceptionState& exception_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8PermissionState>>(
          script_state, exception_state.GetContext());
  auto result = resolver->Promise();

  RequestPermissionImpl(
      descriptor->mode() == V8FileSystemPermissionMode::Enum::kReadwrite,
      WTF::BindOnce(
          [](FileSystemHandle*,
             ScriptPromiseResolver<V8PermissionState>* resolver,
             FileSystemAccessErrorPtr result,
             mojom::blink::PermissionStatus status) {
            // Keep `this` alive so the handle will not be garbage-collected
            // before the promise is resolved.
            if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
              file_system_access_error::Reject(resolver, *result);
              return;
            }
            resolver->Resolve(ToV8PermissionState(status));
          },
          WrapPersistent(this), WrapPersistent(resolver)));

  return result;
}

ScriptPromise<IDLUndefined> FileSystemHandle::move(
    ScriptState* script_state,
    const String& new_entry_name,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto result = resolver->Promise();

  MoveImpl(
      mojo::NullRemote(), new_entry_name,
      WTF::BindOnce(
          [](FileSystemHandle* handle, const String& new_name,
             ScriptPromiseResolver<IDLUndefined>* resolver,
             FileSystemAccessErrorPtr result) {
            if (result->status == mojom::blink::FileSystemAccessStatus::kOk) {
              handle->name_ = new_name;
            }
            file_system_access_error::ResolveOrReject(resolver, *result);
          },
          WrapPersistent(this), new_entry_name, WrapPersistent(resolver)));

  return result;
}

ScriptPromise<IDLUndefined> FileSystemHandle::move(
    ScriptState* script_state,
    FileSystemDirectoryHandle* destination_directory,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto result = resolver->Promise();

  MoveImpl(
      destination_directory->Transfer(), name_,
      WTF::BindOnce(
          [](FileSystemHandle*, ScriptPromiseResolver<IDLUndefined>* resolver,
             FileSystemAccessErrorPtr result) {
            // Keep `this` alive so the handle will not be
            // garbage-collected before the promise is resolved.
            file_system_access_error::ResolveOrReject(resolver, *result);
          },
          WrapPersistent(this), WrapPersistent(resolver)));

  return result;
}

ScriptPromise<IDLUndefined> FileSystemHandle::move(
    ScriptState* script_state,
    FileSystemDirectoryHandle* destination_directory,
    const String& new_entry_name,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto result = resolver->Promise();

  MoveImpl(
      destination_directory->Transfer(), new_entry_name,
      WTF::BindOnce(
          [](FileSystemHandle* handle, const String& new_name,
             ScriptPromiseResolver<IDLUndefined>* resolver,
             FileSystemAccessErrorPtr result) {
            if (result->status == mojom::blink::FileSystemAccessStatus::kOk) {
              handle->name_ = new_name;
            }
            file_system_access_error::ResolveOrReject(resolver, *result);
          },
          WrapPersistent(this), new_entry_name, WrapPersistent(resolver)));

  return result;
}

ScriptPromise<IDLUndefined> FileSystemHandle::remove(
    ScriptState* script_state,
    const FileSystemRemoveOptions* options,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto result = resolver->Promise();

  RemoveImpl(options, WTF::BindOnce(
                          [](FileSystemHandle*,
                             ScriptPromiseResolver<IDLUndefined>* resolver,
                             FileSystemAccessErrorPtr result) {
                            // Keep `this` alive so the handle will not be
                            // garbage-collected before the promise is resolved.
                            file_system_access_error::ResolveOrReject(resolver,
                                                                      *result);
                          },
                          WrapPersistent(this), WrapPersistent(resolver)));

  return result;
}

ScriptPromise<IDLBoolean> FileSystemHandle::isSameEntry(
    ScriptState* script_state,
    FileSystemHandle* other,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(
      script_state, exception_state.GetContext());
  auto result = resolver->Promise();

  IsSameEntryImpl(
      other->Transfer(),
      WTF::BindOnce(
          [](FileSystemHandle*, ScriptPromiseResolver<IDLBoolean>* resolver,
             FileSystemAccessErrorPtr result, bool same) {
            // Keep `this` alive so the handle will not be garbage-collected
            // before the promise is resolved.
            if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
              file_system_access_error::Reject(resolver, *result);
              return;
            }
            resolver->Resolve(same);
          },
          WrapPersistent(this), WrapPersistent(resolver)));
  return result;
}

ScriptPromise<IDLUSVString> FileSystemHandle::getUniqueId(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUSVString>>(
      script_state, exception_state.GetContext());
  auto result = resolver->Promise();

  GetUniqueIdImpl(WTF::BindOnce(
      [](FileSystemHandle*, ScriptPromiseResolver<IDLUSVString>* resolver,
         FileSystemAccessErrorPtr result, const WTF::String& id) {
        // Keep `this` alive so the handle will not be garbage-collected
        // before the promise is resolved.
        if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
          file_system_access_error::Reject(resolver, *result);
          return;
        }

        resolver->Resolve(std::move(id));
      },
      WrapPersistent(this), WrapPersistent(resolver)));
  return result;
}

ScriptPromise<IDLSequence<FileSystemCloudIdentifier>>
FileSystemHandle::getCloudIdentifiers(ScriptState* script_state,
                                      ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLSequence<FileSystemCloudIdentifier>>>(
      script_state, exception_state.GetContext());
  auto result = resolver->Promise();

  GetCloudIdentifiersImpl(WTF::BindOnce(
      [](FileSystemHandle*,
         ScriptPromiseResolver<IDLSequence<FileSystemCloudIdentifier>>*
             resolver,
         FileSystemAccessErrorPtr result,
         Vector<mojom::blink::FileSystemAccessCloudIdentifierPtr>
             cloud_identifiers) {
        // Keep `this` alive so the handle will not be garbage-collected
        // before the promise is resolved.
        if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
          file_system_access_error::Reject(resolver, *result);
          return;
        }

        HeapVector<Member<FileSystemCloudIdentifier>> return_values;
        return_values.ReserveInitialCapacity(cloud_identifiers.size());
        for (auto& cloud_identifier : cloud_identifiers) {
          FileSystemCloudIdentifier* return_value =
              FileSystemCloudIdentifier::Create();
          return_value->setProviderName(cloud_identifier->provider_name);
          return_value->setId(cloud_identifier->id);
          return_values.push_back(return_value);
        }
        resolver->Resolve(return_values);
      },
      WrapPersistent(this), WrapPersistent(resolver)));
  return result;
}

void FileSystemHandle::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
