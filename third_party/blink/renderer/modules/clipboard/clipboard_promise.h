// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_PROMISE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_PROMISE_H_

#include <utility>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_item.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_writer.h"

namespace blink {

class ScriptPromiseResolver;

class ClipboardPromise final : public GarbageCollected<ClipboardPromise>,
                               public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(ClipboardPromise);

 public:
  // Creates promise to execute Clipboard API functions off the main thread.
  static ScriptPromise CreateForRead(ScriptState*);
  static ScriptPromise CreateForReadText(ScriptState*);
  static ScriptPromise CreateForWrite(ScriptState*,
                                      const HeapVector<Member<ClipboardItem>>&);
  static ScriptPromise CreateForWriteText(ScriptState*, const String&);

  explicit ClipboardPromise(ScriptState*);
  virtual ~ClipboardPromise();

  // Completes current write and starts next write.
  void CompleteWriteRepresentation();
  // For rejections originating from ClipboardWriter.
  void RejectFromReadOrDecodeFailure();

  void Trace(blink::Visitor*) override;

 private:
  // Called to begin writing a type.
  void StartWriteRepresentation();

  // Checks Read/Write permission (interacting with PermissionService).
  void HandleRead();
  void HandleReadText();
  void HandleWrite(HeapVector<Member<ClipboardItem>>*);
  void HandleWriteText(const String&);

  // Reads/Writes after permission check.
  void HandleReadWithPermission(mojom::blink::PermissionStatus);
  void HandleReadTextWithPermission(mojom::blink::PermissionStatus);
  void HandleWriteWithPermission(mojom::blink::PermissionStatus);
  void HandleWriteTextWithPermission(mojom::blink::PermissionStatus);

  // Checks for permissions (interacting with PermissionService).
  mojom::blink::PermissionService* GetPermissionService();
  void RequestPermission(
      mojom::blink::PermissionName permission,
      base::OnceCallback<void(::blink::mojom::PermissionStatus)> callback);

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner();
  bool IsFocusedDocument(ExecutionContext*);

  Member<ScriptState> script_state_;
  Member<ScriptPromiseResolver> script_promise_resolver_;

  std::unique_ptr<ClipboardWriter> clipboard_writer_;
  // Checks for Read and Write permission.
  mojo::Remote<mojom::blink::PermissionService> permission_service_;

  // Only for use in writeText().
  String plain_text_;
  HeapVector<std::pair<String, Member<Blob>>> clipboard_item_data_;
  // Index of clipboard representation currently being processed.
  wtf_size_t clipboard_representation_index_;

  // Because v8 is thread-hostile, ensures that all interactions with
  // ScriptState and ScriptPromiseResolver occur on the main thread.
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ClipboardPromise);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_PROMISE_H_
