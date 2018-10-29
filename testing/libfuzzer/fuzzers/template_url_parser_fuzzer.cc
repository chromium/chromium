// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <random>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_parser.h"

class PseudoRandomFilter : public TemplateURLParser::ParameterFilter {
 public:
  explicit PseudoRandomFilter(uint32_t seed) : generator_(seed), pool_(0, 1) {}
  ~PseudoRandomFilter() override = default;

  bool KeepParameter(const std::string&, const std::string&) override {
    // Return true 254/255 times, ie: as if pool_ only returned uint8_t.
    return pool_(generator_) % (UINT8_MAX + 1);
  }

 private:
  std::mt19937 generator_;
  // Use a uint16_t here instead of uint8_t because uniform_int_distribution
  // does not support 8 bit types on Windows.
  std::uniform_int_distribution<uint16_t> pool_;
};

struct FuzzerFixedParams {
  uint32_t seed_;
};

base::AtExitManager at_exit_manager;  // used by ICU integration

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  CHECK(base::i18n::InitializeICU());
  CHECK(base::CommandLine::Init(*argc, *argv));
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < sizeof(FuzzerFixedParams)) {
    return 0;
  }
  const FuzzerFixedParams* params =
      reinterpret_cast<const FuzzerFixedParams*>(data);
  size -= sizeof(FuzzerFixedParams);
  const char* char_data = reinterpret_cast<const char*>(params + 1);
  PseudoRandomFilter filter(params->seed_);
  TemplateURLParser::Parse(SearchTermsData(), char_data, size, &filter);
  return 0;
}
