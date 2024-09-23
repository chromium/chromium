// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <string>

#include "net/http/transport_security_state.h"

namespace net {

class TransportSecurityStateStaticFuzzer {
 public:
  bool FuzzStaticDomainState(TransportSecurityState* state,
                             const std::string& input) {
    state->enable_static_pins_ = true;
    TransportSecurityState::STSState sts_result;
    TransportSecurityState::PKPState pkp_result;
    return state->GetStaticSTSState(input, &sts_result) ||
           state->GetStaticPKPState(input, &pkp_result);
  }
};

}  // namespace net

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string input(reinterpret_cast<const char*>(data), size);

  net::TransportSecurityStateStaticFuzzer helper;
  net::TransportSecurityState state;

  helper.FuzzStaticDomainState(&state, input);

  return 0;
}
