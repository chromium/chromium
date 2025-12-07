// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

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
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

DEFINE_TEXT_PROTO_FUZZER(const wc_fuzzer::AudioDataCopyToCase& proto) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  test::TaskEnvironment task_environment;
  auto page_holder = std::make_unique<DummyPageHolder>();
  page_holder->GetFrame().GetSettings()->SetScriptEnabled(true);

  ScriptState* script_state =
      ToScriptStateForMainWorld(&page_holder->GetFrame());
  ScriptState::Scope scope(script_state);

  AudioData* audio_data = MakeAudioData(script_state, proto.audio_data());
  if (!audio_data)
    return;

  AudioDataCopyToOptions* options =
      MakeAudioDataCopyToOptions(proto.copy_to().options());

  // Check allocationSize().
  audio_data->allocationSize(options, IGNORE_EXCEPTION_FOR_TESTING);

  AllowSharedBufferSource* destination =
      MakeAllowSharedBufferSource(proto.copy_to().destination()).source;
  DCHECK(destination);

  // The returned promise will be fulfilled synchronously since the source frame
  // is memory-backed.
  // TODO(chcunningham): Wait for promise resolution.
  audio_data->copyTo(destination, options, IGNORE_EXCEPTION_FOR_TESTING);
}

}  // namespace blink
