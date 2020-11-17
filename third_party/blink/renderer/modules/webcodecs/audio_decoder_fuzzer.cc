// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_frame_output_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_codecs_error_callback.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_decoder.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_audio_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/fuzzer_inputs.pb.h"
#include "third_party/blink/renderer/modules/webcodecs/fuzzer_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#include <string>

namespace blink {

DEFINE_TEXT_PROTO_FUZZER(
    const wc_fuzzer::AudioDecoderApiInvocationSequence& proto) {
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

    Persistent<FakeFunction> error_function =
        FakeFunction::Create(script_state, "error");
    Persistent<V8WebCodecsErrorCallback> error_callback =
        V8WebCodecsErrorCallback::Create(error_function->Bind());
    Persistent<FakeFunction> output_function =
        FakeFunction::Create(script_state, "output");
    Persistent<V8AudioFrameOutputCallback> output_callback =
        V8AudioFrameOutputCallback::Create(output_function->Bind());

    Persistent<AudioDecoderInit> audio_decoder_init =
        MakeGarbageCollected<AudioDecoderInit>();
    audio_decoder_init->setError(error_callback);
    audio_decoder_init->setOutput(output_callback);

    Persistent<AudioDecoder> audio_decoder = AudioDecoder::Create(
        script_state, audio_decoder_init, IGNORE_EXCEPTION_FOR_TESTING);

    for (auto& invocation : proto.invocations()) {
      switch (invocation.Api_case()) {
        case wc_fuzzer::AudioDecoderApiInvocation::kConfigure:
          audio_decoder->configure(
              MakeAudioDecoderConfig(invocation.configure()),
              IGNORE_EXCEPTION_FOR_TESTING);
          break;
        case wc_fuzzer::AudioDecoderApiInvocation::kDecode:
          audio_decoder->decode(
              MakeEncodedAudioChunk(invocation.decode().chunk()),
              IGNORE_EXCEPTION_FOR_TESTING);
          break;
        case wc_fuzzer::AudioDecoderApiInvocation::kFlush: {
          // TODO(https://crbug.com/1119253): Fuzz whether to await resolution
          // of the flush promise.
          audio_decoder->flush(IGNORE_EXCEPTION_FOR_TESTING);
          break;
        }
        case wc_fuzzer::AudioDecoderApiInvocation::kReset:
          audio_decoder->reset(IGNORE_EXCEPTION_FOR_TESTING);
          break;
        case wc_fuzzer::AudioDecoderApiInvocation::kClose:
          audio_decoder->close(IGNORE_EXCEPTION_FOR_TESTING);
          break;
        case wc_fuzzer::AudioDecoderApiInvocation::API_NOT_SET:
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
