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
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"

namespace blink {

class DOMException;
class ExceptionState;
class ScriptState;
class ImageDecodeOptions;
class ImageDecoderInit;
class ImageDecodeResult;
class ImageTrackList;
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

  static ScriptPromise isTypeSupported(ScriptState*, String type);

  // image_decoder.idl implementation.
  ScriptPromise decode(const ImageDecodeOptions* options = nullptr);
  ScriptPromise decodeMetadata();
  void reset(DOMException* exception = nullptr);
  void close();
  String type() const;
  bool complete() const;
  ImageTrackList& tracks() const;

  // BytesConsumer::Client implementation.
  void OnStateChange() override;
  String DebugName() const override;

  // GarbageCollected override.
  void Trace(Visitor*) const override;

  // ExecutionContextLifecycleObserver override.
  void ContextDestroyed() override;

  // ScriptWrappable override.
  bool HasPendingActivity() const override;

  // Called by ImageTrack to change the current track.
  void UpdateSelectedTrack();

 private:
  void CreateImageDecoder();

  void MaybeSatisfyPendingDecodes();
  void MaybeSatisfyPendingMetadataDecodes();
  void MaybeUpdateMetadata();

  // Returns false if the decoder was constructed with an ArrayBuffer or
  // ArrayBufferView that has since been neutered.
  bool HasValidEncodedData() const;

  void AbortPendingDecodes(DOMException* exception);

  Member<ScriptState> script_state_;

  // Used when a ReadableStream is provided.
  Member<ReadableStreamBytesConsumer> consumer_;
  scoped_refptr<SharedBuffer> stream_buffer_;

  // Used when all data is provided at construction time.
  scoped_refptr<SegmentReader> segment_reader_;

  // Construction parameters.
  Member<const ImageDecoderInit> init_data_;
  ImageDecoder::AlphaOption alpha_option_ = ImageDecoder::kAlphaPremultiplied;
  ColorBehavior color_behavior_ = ColorBehavior::Tag();
  SkISize desired_size_;

  // Copy of |preferAnimation| from |init_data_|.
  base::Optional<bool> prefer_animation_;

  // Currently configured AnimationOption for |decoder_|.
  ImageDecoder::AnimationOption animation_option_ =
      ImageDecoder::AnimationOption::kUnspecified;

  bool data_complete_ = false;

  bool closed_ = false;

  std::unique_ptr<ImageDecoder> decoder_;
  String mime_type_;
  Member<ImageTrackList> tracks_;

  // Pending decode() requests.
  struct DecodeRequest : public GarbageCollected<DecodeRequest> {
    DecodeRequest(ScriptPromiseResolver* resolver,
                  uint32_t frame_index,
                  bool complete_frames_only);
    void Trace(Visitor*) const;

    Member<ScriptPromiseResolver> resolver;
    uint32_t frame_index;
    bool complete_frames_only;
    Member<ImageDecodeResult> result;
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
