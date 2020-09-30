// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_NATIVE_FILE_SYSTEM_DIRECTORY_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_NATIVE_FILE_SYSTEM_DIRECTORY_ITERATOR_H_

#include "base/files/file.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_directory_handle.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_error.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"

namespace blink {
class NativeFileSystemDirectoryHandle;
class NativeFileSystemHandle;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

class NativeFileSystemDirectoryIterator final
    : public ScriptWrappable,
      public ActiveScriptWrappable<NativeFileSystemDirectoryIterator>,
      public ExecutionContextClient,
      public mojom::blink::NativeFileSystemDirectoryEntriesListener {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Should this iterator returns keys, values or both?
  enum Mode { kKey, kValue, kKeyValue };

  NativeFileSystemDirectoryIterator(NativeFileSystemDirectoryHandle* directory,
                                    Mode mode,
                                    ExecutionContext* execution_context);

  ScriptPromise next(ScriptState*);

  // ScriptWrappable:
  bool HasPendingActivity() const final;

  void Trace(Visitor*) const override;

 private:
  void DidReadDirectory(mojom::blink::NativeFileSystemErrorPtr result,
                        Vector<mojom::blink::NativeFileSystemEntryPtr> entries,
                        bool has_more_entries) override;

  Mode mode_;
  mojom::blink::NativeFileSystemErrorPtr error_;
  bool waiting_for_more_entries_ = true;
  HeapDeque<Member<NativeFileSystemHandle>> entries_;
  Member<ScriptPromiseResolver> pending_next_;
  Member<NativeFileSystemDirectoryHandle> directory_;
  HeapMojoReceiver<mojom::blink::NativeFileSystemDirectoryEntriesListener,
                   NativeFileSystemDirectoryIterator,
                   HeapMojoWrapperMode::kWithoutContextObserver>
      receiver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_NATIVE_FILE_SYSTEM_DIRECTORY_ITERATOR_H_
