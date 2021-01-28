// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_IMAGE_DECODER_EXTERNAL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_IMAGE_DECODER_EXTERNAL_H_

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"

namespace blink {

class ExceptionState;
class ScriptState;
class ImageBitmapOptions;
class ImageDecodeOptions;
class ImageDecoder;
class ImageDecoderInit;
class ImageFrameExternal;
class ImageTrackExternal;
class ReadableStreamBytesConsumer;
class ScriptPromiseResolver;
class SegmentReader;

class MODULES_EXPORT ImageDecoderExternal final
    : public ScriptWrappable,
      public ActiveScriptWrappable<ImageDecoderExternal>,
      public BytesConsumer::Client,
      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ImageDecoderExternal* Create(ScriptState*,
                                      const ImageDecoderInit*,
                                      ExceptionState&);

  ImageDecoderExternal(ScriptState*, const ImageDecoderInit*, ExceptionState&);
  ~ImageDecoderExternal() override;

  static bool canDecodeType(String type);

  using ImageTrackList = HeapVector<Member<ImageTrackExternal>>;

  // image_decoder.idl implementation.
  ScriptPromise decode(const ImageDecodeOptions* options = nullptr);
  ScriptPromise decodeMetadata();
  void selectTrack(uint32_t track_id, ExceptionState&);
  uint32_t frameCount() const;
  String type() const;
  uint32_t repetitionCount() const;
  bool complete() const;
  const ImageTrackList tracks() const;

  // BytesConsumer::Client implementation.
  void OnStateChange() override;
  String DebugName() const override;

  // GarbageCollected override.
  void Trace(Visitor*) const override;

  // ExecutionContextLifecycleObserver override.
  void ContextDestroyed() override;

  // ScriptWrappable override.
  bool HasPendingActivity() const override;

 private:
  void CreateImageDecoder();

  void MaybeSatisfyPendingDecodes();
  void MaybeSatisfyPendingMetadataDecodes();
  void MaybeUpdateMetadata();

  // Returns false if the decoder was constructed with an ArrayBuffer or
  // ArrayBufferView that has since been neutered.
  bool HasValidEncodedData() const;

  Member<ScriptState> script_state_;

  // Used when a ReadableStream is provided.
  Member<ReadableStreamBytesConsumer> consumer_;
  scoped_refptr<SharedBuffer> stream_buffer_;

  // Used when all data is provided at construction time.
  scoped_refptr<SegmentReader> segment_reader_;

  // Construction parameters.
  Member<const ImageDecoderInit> init_data_;
  Member<const ImageBitmapOptions> options_;

  // Copy of |preferAnimation| from |init_data_|. Will be modified based on
  // calls to selectTrack().
  base::Optional<bool> prefer_animation_;

  bool data_complete_ = false;

  std::unique_ptr<ImageDecoder> decoder_;
  String mime_type_;
  uint32_t frame_count_ = 0u;
  uint32_t repetition_count_ = 0u;
  base::Optional<uint32_t> selected_track_id_;
  ImageTrackList tracks_;

  // Pending decode() requests.
  struct DecodeRequest : public GarbageCollected<DecodeRequest> {
    DecodeRequest(ScriptPromiseResolver* resolver,
                  uint32_t frame_index,
                  bool complete_frames_only);
    void Trace(Visitor*) const;

    Member<ScriptPromiseResolver> resolver;
    uint32_t frame_index;
    bool complete_frames_only;
    Member<ImageFrameExternal> result;
    Member<DOMException> exception;
  };
  HeapVector<Member<DecodeRequest>> pending_decodes_;
  HeapVector<Member<ScriptPromiseResolver>> pending_metadata_decodes_;

  // When decode() of incomplete frames has been requested, we need to track the
  // generation id for each SkBitmap that we've handed out. So that we can defer
  // resolution of promises until a new bitmap is generated.
  HashMap<uint32_t,
          uint32_t,
          DefaultHash<uint32_t>::Hash,
          WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>
      incomplete_frames_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_IMAGE_DECODER_EXTERNAL_H_
