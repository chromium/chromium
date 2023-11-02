// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "net/ntlm/ntlm_client.h"
#include "net/ntlm/ntlm_test_data.h"

std::u16string ConsumeRandomLengthString16(FuzzedDataProvider& data_provider,
                                           size_t max_chars) {
  std::string bytes = data_provider.ConsumeRandomLengthString(max_chars * 2);
  return std::u16string(reinterpret_cast<const char16_t*>(bytes.data()),
                        bytes.size() / 2);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider fdp(data, size);
  bool is_v2 = fdp.ConsumeBool();
  uint64_t client_time = fdp.ConsumeIntegral<uint64_t>();
  net::ntlm::NtlmClient client((net::ntlm::NtlmFeatures(is_v2)));

  // Generate the input strings and challenge message. The strings will have a
  // maximum length 1 character longer than the maximum that |NtlmClient| will
  // accept to allow exploring the error cases.
  std::u16string domain =
      ConsumeRandomLengthString16(fdp, net::ntlm::kMaxFqdnLen + 1);
  std::u16string username =
      ConsumeRandomLengthString16(fdp, net::ntlm::kMaxUsernameLen + 1);
  std::u16string password =
      ConsumeRandomLengthString16(fdp, net::ntlm::kMaxPasswordLen + 1);
  std::string hostname =
      fdp.ConsumeRandomLengthString(net::ntlm::kMaxFqdnLen + 1);
  std::string channel_bindings = fdp.ConsumeRandomLengthString(150);
  std::string spn =
      fdp.ConsumeRandomLengthString(net::ntlm::kMaxFqdnLen + 5 + 1);
  std::vector<uint8_t> challenge_msg_bytes =
      fdp.ConsumeRemainingBytes<uint8_t>();

  client.GenerateAuthenticateMessage(
      domain, username, password, hostname, channel_bindings, spn, client_time,
      net::ntlm::test::kClientChallenge, base::make_span(challenge_msg_bytes));
  return 0;
}
