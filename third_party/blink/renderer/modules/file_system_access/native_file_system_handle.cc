// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/native_file_system_handle.h"

#include "third_party/blink/public/mojom/file_system_access/native_file_system_error.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_handle_permission_descriptor.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/modules/file_system_access/native_file_system_directory_handle.h"
#include "third_party/blink/renderer/modules/file_system_access/native_file_system_error.h"
#include "third_party/blink/renderer/modules/file_system_access/native_file_system_file_handle.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
using mojom::blink::NativeFileSystemEntryPtr;
using mojom::blink::NativeFileSystemErrorPtr;

NativeFileSystemHandle::NativeFileSystemHandle(
    ExecutionContext* execution_context,
    const String& name)
    : ExecutionContextClient(execution_context), name_(name) {}

// static
NativeFileSystemHandle* NativeFileSystemHandle::CreateFromMojoEntry(
    mojom::blink::NativeFileSystemEntryPtr e,
    ExecutionContext* execution_context) {
  if (e->entry_handle->is_file()) {
    return MakeGarbageCollected<NativeFileSystemFileHandle>(
        execution_context, e->name, std::move(e->entry_handle->get_file()));
  }
  return MakeGarbageCollected<NativeFileSystemDirectoryHandle>(
      execution_context, e->name, std::move(e->entry_handle->get_directory()));
}

namespace {
String MojoPermissionStatusToString(mojom::blink::PermissionStatus status) {
  switch (status) {
    case mojom::blink::PermissionStatus::GRANTED:
      return "granted";
    case mojom::blink::PermissionStatus::DENIED:
      return "denied";
    case mojom::blink::PermissionStatus::ASK:
      return "prompt";
  }
  NOTREACHED();
  return "denied";
}

}  // namespace

ScriptPromise NativeFileSystemHandle::queryPermission(
    ScriptState* script_state,
    const FileSystemHandlePermissionDescriptor* descriptor) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  QueryPermissionImpl(
      descriptor->mode() == "readwrite",
      WTF::Bind(
          [](ScriptPromiseResolver* resolver,
             mojom::blink::PermissionStatus result) {
            resolver->Resolve(MojoPermissionStatusToString(result));
          },
          WrapPersistent(resolver)));

  return result;
}

ScriptPromise NativeFileSystemHandle::requestPermission(
    ScriptState* script_state,
    const FileSystemHandlePermissionDescriptor* descriptor) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  RequestPermissionImpl(
      descriptor->mode() == "readwrite",
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, NativeFileSystemErrorPtr result,
             mojom::blink::PermissionStatus status) {
            if (result->status != mojom::blink::NativeFileSystemStatus::kOk) {
              native_file_system_error::Reject(resolver, *result);
              return;
            }
            resolver->Resolve(MojoPermissionStatusToString(status));
          },
          WrapPersistent(resolver)));

  return result;
}

ScriptPromise NativeFileSystemHandle::isSameEntry(
    ScriptState* script_state,
    NativeFileSystemHandle* other) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  IsSameEntryImpl(
      other->Transfer(),
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, NativeFileSystemErrorPtr result,
             bool same) {
            if (result->status != mojom::blink::NativeFileSystemStatus::kOk) {
              native_file_system_error::Reject(resolver, *result);
              return;
            }
            resolver->Resolve(same);
          },
          WrapPersistent(resolver)));
  return result;
}

void NativeFileSystemHandle::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
