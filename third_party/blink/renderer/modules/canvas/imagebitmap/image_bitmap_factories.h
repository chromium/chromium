/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_IMAGEBITMAP_IMAGE_BITMAP_FACTORIES_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_IMAGEBITMAP_IMAGE_BITMAP_FACTORIES_H_

#include <memory>

#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/image_bitmap_source.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader_client.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_source_union.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkImage;

namespace blink {

class Blob;
class ExecutionContext;
class ImageBitmapSource;

class MODULES_EXPORT ImageBitmapFactories final
    : public GarbageCollected<ImageBitmapFactories>,
      public Supplement<ExecutionContext>,
      public NameClient {
 public:
  static const char kSupplementName[];

  static ScriptPromise CreateImageBitmap(ScriptState*,
                                         const ImageBitmapSourceUnion&,
                                         const ImageBitmapOptions*,
                                         ExceptionState&);
  static ScriptPromise CreateImageBitmap(ScriptState*,
                                         const ImageBitmapSourceUnion&,
                                         int sx,
                                         int sy,
                                         int sw,
                                         int sh,
                                         const ImageBitmapOptions*,
                                         ExceptionState&);
  static ScriptPromise CreateImageBitmap(ScriptState*,
                                         ImageBitmapSource*,
                                         base::Optional<IntRect> crop_rect,
                                         const ImageBitmapOptions*,
                                         ExceptionState&);

  // window.createImageBitmap()
  static ScriptPromise createImageBitmap(
      ScriptState* script_state,
      LocalDOMWindow&,
      const ImageBitmapSourceUnion& bitmap_source,
      const ImageBitmapOptions* options,
      ExceptionState& exception_state) {
    return CreateImageBitmap(script_state, bitmap_source, options,
                             exception_state);
  }
  static ScriptPromise createImageBitmap(
      ScriptState* script_state,
      LocalDOMWindow&,
      const ImageBitmapSourceUnion& bitmap_source,
      int sx,
      int sy,
      int sw,
      int sh,
      const ImageBitmapOptions* options,
      ExceptionState& exception_state) {
    return CreateImageBitmap(script_state, bitmap_source, sx, sy, sw, sh,
                             options, exception_state);
  }

  // worker.createImageBitmap()
  static ScriptPromise createImageBitmap(
      ScriptState* script_state,
      WorkerGlobalScope&,
      const ImageBitmapSourceUnion& bitmap_source,
      const ImageBitmapOptions* options,
      ExceptionState& exception_state) {
    return CreateImageBitmap(script_state, bitmap_source, options,
                             exception_state);
  }
  static ScriptPromise createImageBitmap(
      ScriptState* script_state,
      WorkerGlobalScope&,
      const ImageBitmapSourceUnion& bitmap_source,
      int sx,
      int sy,
      int sw,
      int sh,
      const ImageBitmapOptions* options,
      ExceptionState& exception_state) {
    return CreateImageBitmap(script_state, bitmap_source, sx, sy, sw, sh,
                             options, exception_state);
  }

  virtual ~ImageBitmapFactories() = default;

  void Trace(Visitor*) const override;
  const char* NameInHeapSnapshot() const override {
    return "ImageBitmapLoader";
  }

 private:
  class ImageBitmapLoader final : public GarbageCollected<ImageBitmapLoader>,
                                  public ExecutionContextLifecycleObserver,
                                  public FileReaderLoaderClient {
   public:
    static ImageBitmapLoader* Create(ImageBitmapFactories& factory,
                                     base::Optional<IntRect> crop_rect,
                                     const ImageBitmapOptions* options,
                                     ScriptState* script_state) {
      return MakeGarbageCollected<ImageBitmapLoader>(factory, crop_rect,
                                                     script_state, options);
    }

    ImageBitmapLoader(ImageBitmapFactories&,
                      base::Optional<IntRect> crop_rect,
                      ScriptState*,
                      const ImageBitmapOptions*);

    void LoadBlobAsync(Blob*);
    ScriptPromise Promise() { return resolver_->Promise(); }

    void Trace(Visitor*) const override;

    ~ImageBitmapLoader() override;

   private:
    SEQUENCE_CHECKER(sequence_checker_);

    enum ImageBitmapRejectionReason {
      kUndecodableImageBitmapRejectionReason,
      kAllocationFailureImageBitmapRejectionReason,
    };

    void RejectPromise(ImageBitmapRejectionReason);

    void ScheduleAsyncImageBitmapDecoding(ArrayBufferContents);
    void ResolvePromiseOnOriginalThread(sk_sp<SkImage>,
                                        const ImageOrientationEnum);

    // ExecutionContextLifecycleObserver
    void ContextDestroyed() override;

    // FileReaderLoaderClient
    void DidStartLoading() override {}
    void DidReceiveData() override {}
    void DidFinishLoading() override;
    void DidFail(FileErrorCode) override;

    std::unique_ptr<FileReaderLoader> loader_;
    Member<ImageBitmapFactories> factory_;
    Member<ScriptPromiseResolver> resolver_;
    base::Optional<IntRect> crop_rect_;
    Member<const ImageBitmapOptions> options_;
  };

  static ImageBitmapFactories& From(ExecutionContext&);
  static ScriptPromise CreateImageBitmapFromBlob(
      ScriptState*,
      ImageBitmapSource*,
      base::Optional<IntRect> crop_rect,
      const ImageBitmapOptions*);

  void AddLoader(ImageBitmapLoader*);
  void DidFinishLoading(ImageBitmapLoader*);

  HeapHashSet<Member<ImageBitmapLoader>> pending_loaders_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_IMAGEBITMAP_IMAGE_BITMAP_FACTORIES_H_
