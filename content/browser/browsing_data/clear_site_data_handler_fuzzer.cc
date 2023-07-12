// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <set>
#include <string>

#include "base/command_line.h"
#include "content/browser/browsing_data/clear_site_data_handler.h"  // nogncheck
#include "testing/libfuzzer/libfuzzer_exports.h"

namespace content {

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  base::CommandLine::Init(*argc, *argv);
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string header(reinterpret_cast<const char*>(data), size);

  ClearSiteDataTypeSet clear_site_data_types;
  std::set<std::string> storage_buckets_to_remove;
  ClearSiteDataHandler::ConsoleMessagesDelegate delegate_;

  content::ClearSiteDataHandler::ParseHeaderForTesting(
      header, &clear_site_data_types, &storage_buckets_to_remove, &delegate_,
      GURL());

  return 0;
}

}  // namespace content
