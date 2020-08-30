// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/run_loop.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_output_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_codecs_error_callback.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/fuzzer_inputs.pb.h"
#include "third_party/blink/renderer/modules/webcodecs/fuzzer_utils.h"
#include "third_party/blink/renderer/modules/webcodecs/video_encoder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#include <string>

#include "third_party/protobuf/src/google/protobuf/text_format.h"

namespace blink {

DEFINE_TEXT_PROTO_FUZZER(
    const wc_fuzzer::VideoEncoderApiInvocationSequence& proto) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  static DummyPageHolder* page_holder = []() {
    auto page_holder = std::make_unique<DummyPageHolder>();
    page_holder->GetFrame().GetSettings()->SetScriptEnabled(true);
    return page_holder.release();
  }();

  // Some Image related classes that use base::Singleton will expect this to
  // exist for registering exit callbacks (e.g. DarkModeImageClassifier).
  base::AtExitManager exit_manager;

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

    Persistent<FakeFunction> error_function =
        FakeFunction::Create(script_state, "error");
    Persistent<V8WebCodecsErrorCallback> error_callback =
        V8WebCodecsErrorCallback::Create(error_function->Bind());
    Persistent<FakeFunction> output_function =
        FakeFunction::Create(script_state, "output");
    Persistent<V8VideoEncoderOutputCallback> output_callback =
        V8VideoEncoderOutputCallback::Create(output_function->Bind());

    Persistent<VideoEncoderInit> video_encoder_init =
        MakeGarbageCollected<VideoEncoderInit>();
    video_encoder_init->setError(error_callback);
    video_encoder_init->setOutput(output_callback);

    Persistent<VideoEncoder> video_encoder = VideoEncoder::Create(
        script_state, video_encoder_init, IGNORE_EXCEPTION_FOR_TESTING);

    for (auto& invocation : proto.invocations()) {
      switch (invocation.Api_case()) {
        case wc_fuzzer::VideoEncoderApiInvocation::kConfigure:
          video_encoder->configure(MakeEncoderConfig(invocation.configure()),
                                   IGNORE_EXCEPTION_FOR_TESTING);
          break;
        case wc_fuzzer::VideoEncoderApiInvocation::kEncode: {
          VideoFrame* frame = MakeVideoFrame(invocation.encode().frame());
          // Often the fuzzer input will be too crazy to produce a valid frame
          // (e.g. bitmap width > bitmap length). In these cases, return early
          // to discourage this sort of fuzzer input. WebIDL doesn't allow
          // callers to pass null, so this is not a real concern.
          if (!frame)
            return;

          video_encoder->encode(
              frame, MakeEncodeOptions(invocation.encode().options()),
              IGNORE_EXCEPTION_FOR_TESTING);
          break;
        }
        case wc_fuzzer::VideoEncoderApiInvocation::kFlush: {
          // TODO(https://crbug.com/1119253): Fuzz whether to await resolution
          // of the flush promise.
          video_encoder->flush(IGNORE_EXCEPTION_FOR_TESTING);
          break;
        }
        case wc_fuzzer::VideoEncoderApiInvocation::kReset:
          video_encoder->reset(IGNORE_EXCEPTION_FOR_TESTING);
          break;
        case wc_fuzzer::VideoEncoderApiInvocation::kClose:
          video_encoder->close(IGNORE_EXCEPTION_FOR_TESTING);
          break;
        case wc_fuzzer::VideoEncoderApiInvocation::API_NOT_SET:
          break;
      }

      // Give other tasks a chance to run (e.g. calling our output callback).
      base::RunLoop().RunUntilIdle();
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
