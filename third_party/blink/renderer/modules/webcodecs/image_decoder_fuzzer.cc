// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_decoder_init.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webcodecs/fuzzer_inputs.pb.h"
#include "third_party/blink/renderer/modules/webcodecs/fuzzer_utils.h"
#include "third_party/blink/renderer/modules/webcodecs/image_decoder_external.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#include <string>

namespace blink {

namespace {

String ToImageOrientation(wc_fuzzer::ImageBitmapOptions_ImageOrientation type) {
  switch (type) {
    case wc_fuzzer::ImageBitmapOptions_ImageOrientation_ORIENTATION_NONE:
      return "none";
    case wc_fuzzer::ImageBitmapOptions_ImageOrientation_FLIPY:
      return "flipY";
  }
}

String ToPremultiplyAlpha(wc_fuzzer::ImageBitmapOptions_PremultiplyAlpha type) {
  switch (type) {
    case wc_fuzzer::ImageBitmapOptions_PremultiplyAlpha_PREMULTIPLY_NONE:
      return "none";
    case wc_fuzzer::ImageBitmapOptions_PremultiplyAlpha_PREMULTIPLY:
      return "premultiply";
    case wc_fuzzer::ImageBitmapOptions_PremultiplyAlpha_PREMULTIPLY_DEFAULT:
      return "default";
  }
}

String ToColorSpaceConversion(
    wc_fuzzer::ImageBitmapOptions_ColorSpaceConversion type) {
  switch (type) {
    case wc_fuzzer::ImageBitmapOptions_ColorSpaceConversion_CS_NONE:
      return "none";
    case wc_fuzzer::ImageBitmapOptions_ColorSpaceConversion_CS_DEFAULT:
      return "default";
  }
}

String ToResizeQuality(wc_fuzzer::ImageBitmapOptions_ResizeQuality type) {
  switch (type) {
    case wc_fuzzer::ImageBitmapOptions_ResizeQuality_PIXELATED:
      return "pixelated";
    case wc_fuzzer::ImageBitmapOptions_ResizeQuality_LOW:
      return "low";
    case wc_fuzzer::ImageBitmapOptions_ResizeQuality_MEDIUM:
      return "medium";
    case wc_fuzzer::ImageBitmapOptions_ResizeQuality_HIGH:
      return "high";
  }
}

}  // namespace

DEFINE_BINARY_PROTO_FUZZER(
    const wc_fuzzer::ImageDecoderApiInvocationSequence& proto) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  static DummyPageHolder* page_holder = []() {
    auto page_holder = std::make_unique<DummyPageHolder>();
    page_holder->GetFrame().GetSettings()->SetScriptEnabled(true);
    return page_holder.release();
  }();

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
  {
    Persistent<ScriptState> script_state =
        ToScriptStateForMainWorld(&page_holder->GetFrame());
    ScriptState::Scope scope(script_state);

    Persistent<ImageDecoderInit> image_decoder_init =
        MakeGarbageCollected<ImageDecoderInit>();
    image_decoder_init->setType(proto.config().type().c_str());
    DOMArrayBuffer* data_copy = DOMArrayBuffer::Create(
        proto.config().data().data(), proto.config().data().size());
    image_decoder_init->setData(
        ArrayBufferOrArrayBufferViewOrReadableStream::FromArrayBuffer(
            data_copy));

    Persistent<ImageBitmapOptions> options = ImageBitmapOptions::Create();
    options->setImageOrientation(
        ToImageOrientation(proto.config().options().image_orientation()));
    options->setPremultiplyAlpha(
        ToPremultiplyAlpha(proto.config().options().premultiply_alpha()));
    options->setColorSpaceConversion(ToColorSpaceConversion(
        proto.config().options().color_space_conversion()));
    options->setResizeWidth(proto.config().options().resize_width());
    options->setResizeHeight(proto.config().options().resize_height());
    options->setResizeQuality(
        ToResizeQuality(proto.config().options().resize_quality()));
    image_decoder_init->setOptions(options);

    image_decoder_init->setPreferAnimation(proto.config().prefer_animation());

    Persistent<ImageDecoderExternal> image_decoder =
        ImageDecoderExternal::Create(script_state, image_decoder_init,
                                     IGNORE_EXCEPTION_FOR_TESTING);

    // Promises will be fulfilled synchronously since we're using an array
    // buffer based source.
    for (auto& invocation : proto.invocations()) {
      switch (invocation.Api_case()) {
        case wc_fuzzer::ImageDecoderApiInvocation::kDecodeImage:
          image_decoder->decode(
              invocation.decode_image().frame_index(),
              invocation.decode_image().complete_frames_only());
          break;
        case wc_fuzzer::ImageDecoderApiInvocation::kDecodeMetadata:
          image_decoder->decodeMetadata();
          break;
        case wc_fuzzer::ImageDecoderApiInvocation::kSelectTrack:
          image_decoder->selectTrack(invocation.select_track().track_id(),
                                     IGNORE_EXCEPTION_FOR_TESTING);
          break;
        case wc_fuzzer::ImageDecoderApiInvocation::API_NOT_SET:
          break;
      }

      // Give other tasks a chance to run (e.g. calling our output callback).
      base::RunLoop().RunUntilIdle();

      // TODO(crbug.com/1166925): Push the same image data incrementally into
      // the fuzzer via a ReadableSource.
    }
  }

  // Request a V8 GC. Oilpan will be invoked by the GC epilogue.
  //
  // Multiple GCs may be required to ensure everything is collected (due to
  // a chain of persistent handles), so some objects may not be collected until
  // a subsequent iteration. This is slow enough as is, so we compromise on one
  // major GC, as opposed to the 5 used in V8GCController for unit tests.
  V8PerIsolateData::MainThreadIsolate()->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
}

}  // namespace blink
