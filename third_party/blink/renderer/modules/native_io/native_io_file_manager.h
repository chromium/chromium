// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_FILE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_FILE_MANAGER_H_

#include "third_party/blink/public/mojom/native_io/native_io.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/native_io/native_io_capacity_tracker.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ExecutionContext;
class ExceptionState;
class ScriptPromiseResolver;
class NativeIOFileSync;
class ScriptState;

class NativeIOFileManager final : public ScriptWrappable,
                                  public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit NativeIOFileManager(
      ExecutionContext*,
      HeapMojoRemote<mojom::blink::NativeIOHost> backend,
      NativeIOCapacityTracker* capacity_tracker);

  NativeIOFileManager(const NativeIOFileManager&) = delete;
  NativeIOFileManager& operator=(const NativeIOFileManager&) = delete;

  // Needed because of the
  // mojo::Remote<blink::mojom::NativeIOHost>
  ~NativeIOFileManager() override;

  ScriptPromise open(ScriptState*, String name, ExceptionState&);
  ScriptPromise Delete(ScriptState*, String name, ExceptionState&);
  ScriptPromise getAll(ScriptState*, ExceptionState&);
  ScriptPromise rename(ScriptState*,
                       String old_name,
                       String new_name,
                       ExceptionState&);
  ScriptPromise requestCapacity(ScriptState*,
                                uint64_t requested_capacity,
                                ExceptionState&);
  ScriptPromise releaseCapacity(ScriptState*,
                                uint64_t released_capacity,
                                ExceptionState&);
  ScriptPromise getRemainingCapacity(ScriptState*, ExceptionState&);

  NativeIOFileSync* openSync(String name, ExceptionState&);
  void deleteSync(String name, ExceptionState&);
  Vector<String> getAllSync(ExceptionState&);
  void renameSync(String old_name, String new_name, ExceptionState&);
  uint64_t requestCapacitySync(uint64_t requested_capacity, ExceptionState&);
  uint64_t releaseCapacitySync(uint64_t released_capacity, ExceptionState&);
  uint64_t getRemainingCapacitySync(ExceptionState&);

  // GarbageCollected
  void Trace(Visitor* visitor) const override;

 private:
  // Checks whether storage access should be allowed in the provided context,
  // and calls the callback with the result.
  void CheckStorageAccessAllowed(ExecutionContext* context,
                                 ScriptPromiseResolver* resolver,
                                 base::OnceCallback<void()> callback);

  // Called after CheckStoraAccessAllowed is done checking access. Calls the
  // callback if access is allowed, rejects through the resolver if not.
  void DidCheckStorageAccessAllowed(ScriptPromiseResolver* resolver,
                                    base::OnceCallback<void()> callback,
                                    bool allowed_access);

  // Checks whether storage access should be allowed in the provided context.
  bool CheckStorageAccessAllowedSync(ExecutionContext* context);

  // Executes the actual open, after preconditions have been checked.
  void OpenImpl(String name, ScriptPromiseResolver* resolver);

  // Executes the actual delete, after preconditions have been checked.
  void DeleteImpl(String name, ScriptPromiseResolver* resolver);

  // Executes the actual getAll, after preconditions have been checked.
  void GetAllImpl(ScriptPromiseResolver* resolver);

  // Executes the actual rename, after preconditions have been checked.
  void RenameImpl(String old_name,
                  String new_name,
                  ScriptPromiseResolver* resolver);

  // Executes the actual requestCapacity, after preconditions have been checked.
  void RequestCapacityImpl(uint64_t requested_capacity,
                           ScriptPromiseResolver* resolver);

  // Executes the actual releaseCapacity, after preconditions have been checked.
  void ReleaseCapacityImpl(uint64_t requested_difference,
                           ScriptPromiseResolver* resolver);

  // Executes the actual getRemainingCapacity, after preconditions have been
  // checked.
  void GetRemainingCapacityImpl(ScriptPromiseResolver* resolver);

  // Called when the mojo backend disconnects.
  void OnBackendDisconnect();

  // Called after the mojo call to OpenFile completed.
  void OnOpenResult(
      ScriptPromiseResolver* resolver,
      DisallowNewWrapper<HeapMojoRemote<mojom::blink::NativeIOFileHost>>*
          backend_file_wrapper,
      base::File backing_file,
      uint64_t backing_file_length,
      mojom::blink::NativeIOErrorPtr open_error);

  // Called after the mojo call to DeleteFile completed.
  void OnDeleteResult(ScriptPromiseResolver* resolver,
                      mojom::blink::NativeIOErrorPtr delete_error,
                      uint64_t deleted_file_size);

  // Called after the mojo call to RequestCapacityChange completed.
  void OnRequestCapacityChangeResult(ScriptPromiseResolver* resolver,
                                     int64_t granted_capacity);

  // Tracks the capacity for this file manager's execution context.
  Member<NativeIOCapacityTracker> capacity_tracker_;

  // Task runner used by NativeIOFile mojo receivers generated by this API.
  const scoped_refptr<base::SequencedTaskRunner> receiver_task_runner_;

  // Wraps an always-on Mojo pipe for sending requests to the storage backend.
  HeapMojoRemote<mojom::blink::NativeIOHost> backend_;

  // Caches results of checking if storage access is allowed.
  absl::optional<bool> storage_access_allowed_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_FILE_MANAGER_H_
