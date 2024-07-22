// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <random>
#include <string>

#include "libxml/parser.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/i18n/icu_util.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_parser.h"
#include "mojo/core/embedder/embedder.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/libfuzzer/libfuzzer_exports.h"

bool PseudoRandomFilter(std::mt19937* generator,
                        std::uniform_int_distribution<uint16_t>* pool,
                        const std::string&,
                        const std::string&) {
  // Return true 254/255 times, ie: as if pool only returned uint8_t.
  return (*pool)(*generator) % (UINT8_MAX + 1);
}

struct FuzzerFixedParams {
  uint32_t seed_;
};

base::AtExitManager at_exit_manager;  // used by ICU integration

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  CHECK(base::i18n::InitializeICU());
  CHECK(base::CommandLine::Init(*argc, *argv));
  return 0;
}

void ignore(void* ctx, const char* msg, ...) {
  // Error handler to avoid error message spam from libxml parser.
}

class Env {
 public:
  Env() : executor_(base::MessagePumpType::IO) {
    mojo::core::Init();
    xmlSetGenericErrorFunc(nullptr, &ignore);
  }

 private:
  base::SingleThreadTaskExecutor executor_;
  data_decoder::test::InProcessDataDecoder data_decoder_;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Env env;
  if (size < sizeof(FuzzerFixedParams)) {
    return 0;
  }

  const FuzzerFixedParams* params =
      reinterpret_cast<const FuzzerFixedParams*>(data);
  size -= sizeof(FuzzerFixedParams);

  std::mt19937 generator(params->seed_);
  // Use a uint16_t here instead of uint8_t because uniform_int_distribution
  // does not support 8 bit types on Windows.
  std::uniform_int_distribution<uint16_t> pool(0, 1);

  base::RunLoop run_loop;

  SearchTermsData search_terms_data;
  std::string string_data(reinterpret_cast<const char*>(params + 1), size);
  TemplateURLParser::ParameterFilter filter =
      base::BindRepeating(&PseudoRandomFilter, base::Unretained(&generator),
                          base::Unretained(&pool));
  TemplateURLParser::Parse(&search_terms_data, string_data, filter,
                           base::BindOnce(
                               [](base::OnceClosure quit_closure,
                                  std::unique_ptr<TemplateURL> ignored) {
                                 std::move(quit_closure).Run();
                               },
                               run_loop.QuitClosure()));

  run_loop.Run();

  return 0;
}
