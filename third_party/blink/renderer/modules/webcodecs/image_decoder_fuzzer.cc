// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybufferallowshared_arraybufferviewallowshared_readablestream.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_decode_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_decoder_init.h"
#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/test_underlying_source.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webcodecs/fuzzer_inputs.pb.h"
#include "third_party/blink/renderer/modules/webcodecs/fuzzer_utils.h"
#include "third_party/blink/renderer/modules/webcodecs/image_decoder_external.h"
#include "third_party/blink/renderer/modules/webcodecs/image_track.h"
#include "third_party/blink/renderer/modules/webcodecs/image_track_list.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

String ToColorSpaceConversion(
    wc_fuzzer::ImageBitmapOptions_ColorSpaceConversion type) {
  switch (type) {
    case wc_fuzzer::ImageBitmapOptions_ColorSpaceConversion_CS_NONE:
      return "none";
    case wc_fuzzer::ImageBitmapOptions_ColorSpaceConversion_CS_DEFAULT:
      return "default";
  }
}

void RunFuzzingLoop(ImageDecoderExternal* image_decoder,
                    const google::protobuf::RepeatedPtrField<
                        wc_fuzzer::ImageDecoderApiInvocation>& invocations) {
  Persistent<ImageDecodeOptions> options = ImageDecodeOptions::Create();
  for (auto& invocation : invocations) {
    switch (invocation.Api_case()) {
      case wc_fuzzer::ImageDecoderApiInvocation::kDecodeImage:
        options->setFrameIndex(invocation.decode_image().frame_index());
        options->setCompleteFramesOnly(
            invocation.decode_image().complete_frames_only());
        image_decoder->decode(options);
        break;
      case wc_fuzzer::ImageDecoderApiInvocation::kDecodeMetadata:
        // Deprecated.
        break;
      case wc_fuzzer::ImageDecoderApiInvocation::kSelectTrack: {
        auto* track = image_decoder->tracks().AnonymousIndexedGetter(
            invocation.select_track().track_id());
        if (track)
          track->setSelected(invocation.select_track().selected());
        break;
      }
      case wc_fuzzer::ImageDecoderApiInvocation::API_NOT_SET:
        break;
    }

    // Give other tasks a chance to run (e.g. calling our output callback).
    base::RunLoop().RunUntilIdle();
  }
}

}  // namespace

DEFINE_BINARY_PROTO_FUZZER(
    const wc_fuzzer::ImageDecoderApiInvocationSequence& proto) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  test::TaskEnvironment task_environment;

  auto page_holder = std::make_unique<DummyPageHolder>();
  page_holder->GetFrame().GetSettings()->SetScriptEnabled(true);

  //
  // NOTE: GC objects that need to survive iterations of the loop below
  // must be Persistent<>!
  //
  // GC may be triggered by the RunLoop().RunUntilIdle() below, which will GC
  // raw pointers on the stack. This is not required in production code because
  // GC typically runs at the top of the stack, or is conservative enough to
  // keep stack pointers alive.
  //

  // Scoping Persistent<> refs so GC can collect these at the end.
  Persistent<ScriptState> script_state =
      ToScriptStateForMainWorld(&page_holder->GetFrame());
  ScriptState::Scope scope(script_state);

  // Fuzz the isTypeSupported() API explicitly.
  ImageDecoderExternal::isTypeSupported(script_state,
                                        proto.config().type().c_str());

  Persistent<ImageDecoderInit> image_decoder_init =
      MakeGarbageCollected<ImageDecoderInit>();
  image_decoder_init->setType(proto.config().type().c_str());
  Persistent<DOMArrayBuffer> data_copy = DOMArrayBuffer::Create(
      proto.config().data().data(), proto.config().data().size());
  image_decoder_init->setData(
      MakeGarbageCollected<V8ImageBufferSource>(data_copy));
  image_decoder_init->setColorSpaceConversion(ToColorSpaceConversion(
      proto.config().options().color_space_conversion()));

  // Limit resize support to a reasonable value to prevent fuzzer oom.
  constexpr uint32_t kMaxDimension = 4096u;
  image_decoder_init->setDesiredWidth(
      std::min(proto.config().options().resize_width(), kMaxDimension));
  image_decoder_init->setDesiredHeight(
      std::min(proto.config().options().resize_height(), kMaxDimension));
  image_decoder_init->setPreferAnimation(proto.config().prefer_animation());

  Persistent<ImageDecoderExternal> image_decoder = ImageDecoderExternal::Create(
      script_state, image_decoder_init, IGNORE_EXCEPTION_FOR_TESTING);

  if (image_decoder) {
    // Promises will be fulfilled synchronously since we're using an array
    // buffer based source.
    RunFuzzingLoop(image_decoder, proto.invocations());

    // Close out underlying decoder to simplify reproduction analysis.
    image_decoder->close();
    image_decoder = nullptr;
    base::RunLoop().RunUntilIdle();

    // Collect what we can after the first fuzzing loop; this keeps memory
    // pressure down during ReadableStream fuzzing.
    script_state->GetIsolate()->RequestGarbageCollectionForTesting(
        v8::Isolate::kFullGarbageCollection);
  }

  Persistent<TestUnderlyingSource> underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  Persistent<ReadableStream> stream =
      ReadableStream::CreateWithCountQueueingStrategy(script_state,
                                                      underlying_source, 0);

  image_decoder_init->setData(
      MakeGarbageCollected<V8ImageBufferSource>(stream));
  image_decoder = ImageDecoderExternal::Create(script_state, image_decoder_init,
                                               IGNORE_EXCEPTION_FOR_TESTING);
  image_decoder_init = nullptr;

  if (image_decoder) {
    // Split the image data into chunks.
    constexpr size_t kNumChunks = 2;
    const size_t chunk_size = (data_copy->ByteLength() + 1) / kNumChunks;
    size_t offset = 0;
    for (size_t i = 0; i < kNumChunks; ++i) {
      RunFuzzingLoop(image_decoder, proto.invocations());

      const size_t current_chunk_size =
          std::min(data_copy->ByteLength() - offset, chunk_size);

      v8::Local<v8::Value> v8_data_array = ToV8Traits<DOMUint8Array>::ToV8(
          script_state,
          DOMUint8Array::Create(data_copy, offset, current_chunk_size));

      underlying_source->Enqueue(
          ScriptValue(script_state->GetIsolate(), v8_data_array));
      offset += chunk_size;
    }

    underlying_source->Close();
    data_copy = nullptr;

    // Run one additional loop after all data has been appended.
    RunFuzzingLoop(image_decoder, proto.invocations());
  }
}

}  // namespace blink
