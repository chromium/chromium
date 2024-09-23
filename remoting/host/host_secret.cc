// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/host/host_secret.h"

#include <stdint.h>

#include <string>

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"

namespace remoting {

namespace {

// 5 digits means 100K possible host secrets with uniform distribution, which
// should be enough for short-term passwords, given that we rate-limit guesses
// in the cloud and expire access codes after a small number of attempts.
const int kHostSecretLength = 5;
const char kHostSecretAlphabet[] = "0123456789";

// Generates cryptographically strong random number in the range [0, max).
int CryptoRandomInt(int max) {
  uint32_t random_int32;
  base::RandBytes(base::byte_span_from_ref(random_int32));
  return random_int32 % max;
}

}  // namespace

std::string GenerateSupportHostSecret() {
  std::string result;
  int alphabet_size = strlen(kHostSecretAlphabet);
  result.resize(kHostSecretLength);
  for (int i = 0; i < kHostSecretLength; ++i) {
    result[i] = kHostSecretAlphabet[CryptoRandomInt(alphabet_size)];
  }
  return result;
}

}  // namespace remoting
