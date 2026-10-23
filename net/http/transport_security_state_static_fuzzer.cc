// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "net/http/transport_security_state.h"

#if BUILDFLAG(CHROME_KEY_PINNING_SUPPORTED)
#include "net/cert/key_pinning_state_chrome_test_util.h"
#endif

namespace net {

class TransportSecurityStateStaticFuzzer {
 public:
  bool FuzzStaticDomainState(TransportSecurityState* state,
                             const std::string& input) {
    TransportSecurityState::STSState sts_result;
    TransportSecurityState::PKPState pkp_result;
    return state->GetStaticSTSState(input, &sts_result) ||
           state->GetPKPState(input, &pkp_result);
  }
};

}  // namespace net

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string input(reinterpret_cast<const char*>(data), size);

#if BUILDFLAG(CHROME_KEY_PINNING_SUPPORTED)
  net::ScopedKeyPinningStateChromeDefaults scoped_static_pinning_enable(
      net::ScopedKeyPinningStateChromeDefaults::kForceEnableProduction);
#endif
  net::TransportSecurityStateStaticFuzzer helper;
  net::TransportSecurityState state;

  helper.FuzzStaticDomainState(&state, input);

  return 0;
}
