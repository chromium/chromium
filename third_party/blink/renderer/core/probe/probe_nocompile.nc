// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

struct ProbeBase { };
class TestProbeSink;

// Generated include should appear after all dependencies.
#include "third_party/blink/renderer/core/probe/test_probes_inl.h"

namespace blink::probe {

void WontCompile() {
  probe::Frobnicate scoped_probe((String()));  // expected-error {{no matching constructor for initialization of 'probe::Frobnicate'}}
}

}  // namespace blink::probe
