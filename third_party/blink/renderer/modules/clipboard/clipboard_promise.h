// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_PROMISE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_PROMISE_H_

#include <utility>

#include "base/sequence_checker.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
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
  static ScriptPromise CreateForRead(ExecutionContext*, ScriptState*);
  static ScriptPromise CreateForReadText(ExecutionContext*, ScriptState*);
  static ScriptPromise CreateForWrite(ExecutionContext*,
                                      ScriptState*,
                                      const HeapVector<Member<ClipboardItem>>&);
  static ScriptPromise CreateForWriteText(ExecutionContext*,
                                          ScriptState*,
                                          const String&);

  ClipboardPromise(ExecutionContext*, ScriptState*);

  ClipboardPromise(const ClipboardPromise&) = delete;
  ClipboardPromise& operator=(const ClipboardPromise&) = delete;

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
  class BlobPromiseResolverFunction;
  void HandlePromiseBlobsWrite(HeapVector<Member<Blob>>* blob_list);
  // Promises to Blobs in the `ClipboardItem` were rejected.
  void RejectBlobPromise(const String& exception_text);
  // Called to begin writing a type.
  void WriteNextRepresentation();

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

  void OnReadAvailableFormatNames(const Vector<String>& format_names);
  void ReadNextRepresentation();
  void ResolveRead();

  // Checks for permissions (interacting with PermissionService).
  mojom::blink::PermissionService* GetPermissionService();
  void RequestPermission(
      mojom::blink::PermissionName permission,
      bool will_be_sanitized,
      base::OnceCallback<void(::blink::mojom::PermissionStatus)> callback);

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner();

  // ExecutionContextLifecycleObserver
  void ContextDestroyed() override;

  Member<ScriptState> script_state_;
  Member<ScriptPromiseResolver> script_promise_resolver_;

  Member<ClipboardWriter> clipboard_writer_;

  // Checks for Read and Write permission.
  HeapMojoRemote<mojom::blink::PermissionService> permission_service_;

  // Only for use in writeText().
  String plain_text_;
  HeapVector<std::pair<String, Member<Blob>>> clipboard_item_data_;
  HeapVector<std::pair<String, ScriptPromise>>
      clipboard_item_data_with_promises_;
  // Index of clipboard representation currently being processed.
  wtf_size_t clipboard_representation_index_;
  // Stores all the custom formats defined in `ClipboardItemOptions`.
  Vector<String> custom_format_items_;
  // Stores the types provided by the web authors.
  Vector<String> clipboard_item_types_;

  // Because v8 is thread-hostile, ensures that all interactions with
  // ScriptState and ScriptPromiseResolver occur on the main thread.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_PROMISE_H_
