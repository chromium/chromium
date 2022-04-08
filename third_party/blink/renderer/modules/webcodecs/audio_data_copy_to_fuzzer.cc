// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webcodecs/fuzzer_inputs.pb.h"
#include "third_party/blink/renderer/modules/webcodecs/fuzzer_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"

#include <string>

namespace blink {

DEFINE_TEXT_PROTO_FUZZER(const wc_fuzzer::AudioDataCopyToCase& proto) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  static DummyPageHolder* page_holder = []() {
    auto page_holder = std::make_unique<DummyPageHolder>();
    page_holder->GetFrame().GetSettings()->SetScriptEnabled(true);
    return page_holder.release();
  }();

  ScriptState* script_state =
      ToScriptStateForMainWorld(&page_holder->GetFrame());
  ScriptState::Scope scope(script_state);

  AudioData* audio_data = MakeAudioData(proto.audio_data());
  if (!audio_data)
    return;

  AudioDataCopyToOptions* options =
      MakeAudioDataCopyToOptions(proto.copy_to().options());

  // Check allocationSize().
  audio_data->allocationSize(options, IGNORE_EXCEPTION_FOR_TESTING);

  AllowSharedBufferSource* destination =
      MakeAllowSharedBufferSource(proto.copy_to().destination());
  DCHECK(destination);

  // The returned promise will be fulfilled synchronously since the source frame
  // is memory-backed.
  // TODO(chcunningham): Wait for promise resolution.
  audio_data->copyTo(destination, options, IGNORE_EXCEPTION_FOR_TESTING);

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
