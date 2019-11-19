// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_ITERATOR_H_

#include "base/files/file.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_directory_handle.mojom-blink.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {
class NativeFileSystemDirectoryHandle;
class NativeFileSystemHandle;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

class NativeFileSystemDirectoryIterator final
    : public ScriptWrappable,
      public ContextLifecycleObserver,
      public mojom::blink::NativeFileSystemDirectoryEntriesListener {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(NativeFileSystemDirectoryIterator);
  USING_PRE_FINALIZER(NativeFileSystemDirectoryIterator, Dispose);

 public:
  NativeFileSystemDirectoryIterator(NativeFileSystemDirectoryHandle* directory,
                                    ExecutionContext* execution_context);

  ScriptPromise next(ScriptState*);

  void Trace(Visitor*) override;

 private:
  void DidReadDirectory(mojom::blink::NativeFileSystemErrorPtr result,
                        Vector<mojom::blink::NativeFileSystemEntryPtr> entries,
                        bool has_more_entries) override;
  void Dispose();

  mojom::blink::NativeFileSystemErrorPtr error_;
  bool waiting_for_more_entries_ = true;
  HeapDeque<Member<NativeFileSystemHandle>> entries_;
  Member<ScriptPromiseResolver> pending_next_;
  Member<NativeFileSystemDirectoryHandle> directory_;
  mojo::Receiver<mojom::blink::NativeFileSystemDirectoryEntriesListener>
      receiver_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_ITERATOR_H_
