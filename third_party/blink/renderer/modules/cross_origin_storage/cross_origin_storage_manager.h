// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CROSS_ORIGIN_STORAGE_CROSS_ORIGIN_STORAGE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CROSS_ORIGIN_STORAGE_CROSS_ORIGIN_STORAGE_MANAGER_H_

#include "third_party/blink/public/mojom/cross_origin_storage/cross_origin_storage.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class CrossOriginStorageGetFileHandleHash;
class CrossOriginStorageGetFileHandleOptions;
class ExceptionState;
class FileSystemFileHandle;
class NavigatorBase;
template <typename IDLResolvedType>
class ScriptPromiseResolver;
class ScriptPromiseResolverBase;
class ScriptState;

class MODULES_EXPORT CrossOriginStorageManager final
    : public ScriptWrappable,
      public Supplement<NavigatorBase> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  static CrossOriginStorageManager* crossOriginStorage(NavigatorBase&);

  explicit CrossOriginStorageManager(NavigatorBase&);

  CrossOriginStorageManager(const CrossOriginStorageManager&) = delete;
  CrossOriginStorageManager& operator=(const CrossOriginStorageManager&) =
      delete;

  // Web-exposed API
  ScriptPromise<FileSystemFileHandle> getFileHandle(
      ScriptState*,
      const CrossOriginStorageGetFileHandleHash* hash,
      const CrossOriginStorageGetFileHandleOptions* options,
      ExceptionState&);

  void Trace(Visitor*) const override;

 private:
  mojom::blink::CrossOriginStorageManager* GetService();
  void OnConnectionError();
  void OnGetFileHandleComplete(
      ScriptPromiseResolver<FileSystemFileHandle>* resolver,
      const String& name,
      mojom::blink::FileSystemAccessErrorPtr result,
      mojo::PendingRemote<mojom::blink::FileSystemAccessFileHandle>
          file_handle);

  HeapMojoRemote<mojom::blink::CrossOriginStorageManager> service_;
  HeapHashSet<Member<ScriptPromiseResolverBase>> pending_resolvers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CROSS_ORIGIN_STORAGE_CROSS_ORIGIN_STORAGE_MANAGER_H_
