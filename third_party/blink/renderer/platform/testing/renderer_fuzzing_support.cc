// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer_fuzzing_support.h"

#include "testing/libfuzzer/renderer_fuzzing/renderer_fuzzing.h"

namespace blink {

// static
void RendererFuzzingSupport::Run(
    const blink::BrowserInterfaceBrokerProxy* context_interface_broker_proxy,
    blink::ThreadSafeBrowserInterfaceBrokerProxy*
        process_interface_broker_proxy,
    const std::string& fuzzer_id,
    std::vector<uint8_t>&& input,
    base::OnceClosure done_closure) {
  RendererFuzzing::Run(context_interface_broker_proxy,
                       process_interface_broker_proxy, fuzzer_id,
                       std::move(input), std::move(done_closure));
}

}  // namespace blink
