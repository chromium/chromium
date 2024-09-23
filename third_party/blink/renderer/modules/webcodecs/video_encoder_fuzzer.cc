// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_encoder.h"

#include <string>

#include "base/run_loop.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk_output_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_init.h"
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
#include "third_party/protobuf/src/google/protobuf/text_format.h"

namespace blink {

DEFINE_TEXT_PROTO_FUZZER(
    const wc_fuzzer::VideoEncoderApiInvocationSequence& proto) {
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
  Persistent<V8EncodedVideoChunkOutputCallback> output_callback =
      V8EncodedVideoChunkOutputCallback::Create(output_function->V8Function());

  Persistent<VideoEncoderInit> video_encoder_init =
      MakeGarbageCollected<VideoEncoderInit>();
  video_encoder_init->setError(error_callback);
  video_encoder_init->setOutput(output_callback);

  Persistent<VideoEncoder> video_encoder = VideoEncoder::Create(
      script_state, video_encoder_init, IGNORE_EXCEPTION_FOR_TESTING);

  if (video_encoder) {
    for (auto& invocation : proto.invocations()) {
      switch (invocation.Api_case()) {
        case wc_fuzzer::VideoEncoderApiInvocation::kConfigure: {
          VideoEncoderConfig* config =
              MakeVideoEncoderConfig(invocation.configure());

          // Use the same config to fuzz isConfigSupported().
          VideoEncoder::isConfigSupported(script_state, config,
                                          IGNORE_EXCEPTION_FOR_TESTING);

          video_encoder->configure(config, IGNORE_EXCEPTION_FOR_TESTING);
          break;
        }
        case wc_fuzzer::VideoEncoderApiInvocation::kEncode: {
          VideoFrame* frame;
          switch (invocation.encode().Frames_case()) {
            case wc_fuzzer::EncodeVideo::kFrame:
              frame = MakeVideoFrame(script_state, invocation.encode().frame());
              break;
            case wc_fuzzer::EncodeVideo::kFrameFromBuffer:
              frame = MakeVideoFrame(script_state,
                                     invocation.encode().frame_from_buffer());
              break;
            default:
              frame = nullptr;
              break;
          }

          // Often the fuzzer input will be too crazy to produce a valid frame
          // (e.g. bitmap width > bitmap length). In these cases, return early
          // to discourage this sort of fuzzer input. WebIDL doesn't allow
          // callers to pass null, so this is not a real concern.
          if (!frame) {
            return;
          }

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
}

}  // namespace blink
