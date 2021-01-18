// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_PROMISE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_PROMISE_H_

#include <utility>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_item.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_reader.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"

namespace blink {

class ClipboardWriter;
class ScriptPromiseResolver;
class LocalFrame;
class ExecutionContext;
class ClipboardItemOptions;

class ClipboardPromise final : public GarbageCollected<ClipboardPromise>,
                               public ExecutionContextLifecycleObserver {
 public:
  // Creates promise to execute Clipboard API functions off the main thread.
  static ScriptPromise CreateForRead(ExecutionContext*,
                                     ScriptState*,
                                     ClipboardItemOptions*);
  static ScriptPromise CreateForReadText(ExecutionContext*, ScriptState*);
  static ScriptPromise CreateForWrite(ExecutionContext*,
                                      ScriptState*,
                                      const HeapVector<Member<ClipboardItem>>&);
  static ScriptPromise CreateForWriteText(ExecutionContext*,
                                          ScriptState*,
                                          const String&);

  ClipboardPromise(ExecutionContext*, ScriptState*);
  ~ClipboardPromise() override;

  // Completes current write and starts next write.
  void CompleteWriteRepresentation();
  // For rejections originating from ClipboardWriter.
  void RejectFromReadOrDecodeFailure();

  // Adds the blob to the clipboard items.
  void OnRead(Blob* blob);

  LocalFrame* GetLocalFrame() const;

  void Trace(Visitor*) const override;

 private:
  // Called to begin writing a type.
  void WriteNextRepresentation();

  // Checks Read/Write permission (interacting with PermissionService).
  void HandleRead(ClipboardItemOptions*);
  void HandleReadText();
  void HandleWrite(HeapVector<Member<ClipboardItem>>*);
  void HandleWriteText(const String&);

  // Reads/Writes after permission check.
  void HandleReadWithPermission(mojom::blink::PermissionStatus);
  void HandleReadTextWithPermission(mojom::blink::PermissionStatus);
  void HandleWriteWithPermission(mojom::blink::PermissionStatus);
  void HandleWriteTextWithPermission(mojom::blink::PermissionStatus);

  void OnReadAvailableFormatNames(const Vector<String>& format_names);
  void ReadNextRepresentation();
  void OnRawRead(mojo_base::BigBuffer data);
  void ResolveRead();

  // Checks for permissions (interacting with PermissionService).
  mojom::blink::PermissionService* GetPermissionService();
  void RequestPermission(
      mojom::blink::PermissionName permission,
      bool allow_without_sanitization,
      base::OnceCallback<void(::blink::mojom::PermissionStatus)> callback);

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner();

  // ExecutionContextLifecycleObserver
  void ContextDestroyed() override;

  Member<ScriptState> script_state_;
  Member<ScriptPromiseResolver> script_promise_resolver_;

  Member<ClipboardWriter> clipboard_writer_;

  // Checks for Read and Write permission.
  HeapMojoRemote<mojom::blink::PermissionService,
                 HeapMojoWrapperMode::kWithoutContextObserver>
      permission_service_;

  // Only for use in writeText().
  String plain_text_;
  HeapVector<std::pair<String, Member<Blob>>> clipboard_item_data_;
  bool is_raw_;  // Corresponds to allowWithoutSanitization in ClipboardItem.
  // Index of clipboard representation currently being processed.
  wtf_size_t clipboard_representation_index_;

  // Because v8 is thread-hostile, ensures that all interactions with
  // ScriptState and ScriptPromiseResolver occur on the main thread.
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ClipboardPromise);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_PROMISE_H_
