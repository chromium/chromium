// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_decoder.h"

#include <string>

#include "base/run_loop.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_output_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_webcodecs_error_callback.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/fuzzer_inputs.pb.h"
#include "third_party/blink/renderer/modules/webcodecs/fuzzer_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

DEFINE_TEXT_PROTO_FUZZER(
    const wc_fuzzer::VideoDecoderApiInvocationSequence& proto) {
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

  Persistent<ScriptFunction> error_function =
      MakeGarbageCollected<ScriptFunction>(
          script_state, MakeGarbageCollected<FakeFunction>("error"));
  Persistent<V8WebCodecsErrorCallback> error_callback =
      V8WebCodecsErrorCallback::Create(error_function->V8Function());
  Persistent<ScriptFunction> output_function =
      MakeGarbageCollected<ScriptFunction>(
          script_state, MakeGarbageCollected<FakeFunction>("output"));
  Persistent<V8VideoFrameOutputCallback> output_callback =
      V8VideoFrameOutputCallback::Create(output_function->V8Function());

  Persistent<VideoDecoderInit> video_decoder_init =
      MakeGarbageCollected<VideoDecoderInit>();
  video_decoder_init->setError(error_callback);
  video_decoder_init->setOutput(output_callback);

  Persistent<VideoDecoder> video_decoder = VideoDecoder::Create(
      script_state, video_decoder_init, IGNORE_EXCEPTION_FOR_TESTING);

  if (video_decoder) {
    for (auto& invocation : proto.invocations()) {
      switch (invocation.Api_case()) {
        case wc_fuzzer::VideoDecoderApiInvocation::kConfigure: {
          VideoDecoderConfig* config =
              MakeVideoDecoderConfig(invocation.configure());

          // Use the same config to fuzz isConfigSupported().
          VideoDecoder::isConfigSupported(script_state, config,
                                          IGNORE_EXCEPTION_FOR_TESTING);

          video_decoder->configure(config, IGNORE_EXCEPTION_FOR_TESTING);
          break;
        }
        case wc_fuzzer::VideoDecoderApiInvocation::kDecode:
          video_decoder->decode(
              MakeEncodedVideoChunk(script_state, invocation.decode().chunk()),
              IGNORE_EXCEPTION_FOR_TESTING);
          break;
        case wc_fuzzer::VideoDecoderApiInvocation::kFlush: {
          // TODO(https://crbug.com/1119253): Fuzz whether to await resolution
          // of the flush promise.
          video_decoder->flush(IGNORE_EXCEPTION_FOR_TESTING);
          break;
        }
        case wc_fuzzer::VideoDecoderApiInvocation::kReset:
          video_decoder->reset(IGNORE_EXCEPTION_FOR_TESTING);
          break;
        case wc_fuzzer::VideoDecoderApiInvocation::kClose:
          video_decoder->close(IGNORE_EXCEPTION_FOR_TESTING);
          break;
        case wc_fuzzer::VideoDecoderApiInvocation::API_NOT_SET:
          break;
      }

      // Give other tasks a chance to run (e.g. calling our output callback).
      base::RunLoop().RunUntilIdle();
    }
  }
}

}  // namespace blink
