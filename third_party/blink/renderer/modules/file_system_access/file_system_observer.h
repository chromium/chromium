// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_OBSERVER_H_

#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_observer.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_observer_host.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_observer_callback.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {
class ExecutionContext;
class FileSystemHandle;
class FileSystemObserverObserveOptions;
class ScriptPromiseResolver;

class FileSystemObserver : public ScriptWrappable,
                           public mojom::blink::FileSystemAccessObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static FileSystemObserver* Create(ScriptState*,
                                    V8FileSystemObserverCallback* callback,
                                    ExceptionState&);

  FileSystemObserver(
      ExecutionContext* context,
      V8FileSystemObserverCallback* callback,
      mojo::PendingRemote<mojom::blink::FileSystemAccessObserverHost>
          host_remote);

  ScriptPromise observe(ScriptState*,
                        FileSystemHandle* handle,
                        FileSystemObserverObserveOptions* options,
                        ExceptionState&);
  void unobserve(FileSystemHandle* handle);
  void disconnect();

  // blink::mojom::blink::FileSystemAccessObserver
  void OnFileChanges(WTF::Vector<mojom::blink::FileSystemAccessChangePtr>
                         mojo_changes) override;

  void Trace(Visitor* visitor) const override;

 private:
  // TODO(https://crbug.com/1019297): Extend
  // ExecutionContextLifecycleStateObserver to pause file system change
  // notifications while the page is frozen and to destroy `this` when the
  // ExecutionContext is destroyed.

  void DidObserve(ScriptPromiseResolver* resolver,
                  mojom::blink::FileSystemAccessErrorPtr result,
                  mojo::PendingReceiver<mojom::blink::FileSystemAccessObserver>
                      observer_receiver);

  SEQUENCE_CHECKER(sequence_checker_);

  Member<ExecutionContext> execution_context_;
  Member<V8FileSystemObserverCallback> callback_;

  // TODO(https://crbug.com/1019297): Add a queue of records.

  // Each observer connection corresponds to a file system watch set up by
  // calling `observe()`.
  HeapMojoReceiverSet<mojom::blink::FileSystemAccessObserver,
                      FileSystemObserver>
      observer_receivers_;

  HeapMojoRemote<mojom::blink::FileSystemAccessObserverHost> host_remote_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_OBSERVER_H_
