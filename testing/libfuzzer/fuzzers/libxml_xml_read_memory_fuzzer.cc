// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cassert>
#include <cstddef>
#include <cstdint>

#include <functional>
#include <limits>
#include <string>

#include "libxml/parser.h"
#include "libxml/xmlsave.h"

void ignore (void* ctx, const char* msg, ...) {
  // Error handler to avoid spam of error messages from libxml parser.
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  xmlSetGenericErrorFunc(NULL, &ignore);

  // Test default empty options value and one random combination of flags.
  const std::string data_string(reinterpret_cast<const char*>(data), size);
  const std::size_t data_hash = std::hash<std::string>()(data_string);
  const int max_option_value = std::numeric_limits<int>::max();
  // Disable XML_PARSE_HUGE to avoid stack overflow.  http://crbug.com/738947.
  // Disable XML_PARSE_NOENT, XML_PARSE_DTD[LOAD|ATTR|VALID] to avoid timeout
  // loading external entity from stdin. http://crbug.com/755142.
  const int random_option = data_hash & max_option_value & ~XML_PARSE_NOENT &
                            ~XML_PARSE_DTDLOAD & ~XML_PARSE_DTDATTR &
                            ~XML_PARSE_DTDVALID & ~XML_PARSE_HUGE;
  const int options[] = {0, random_option};

  for (const auto option_value : options) {
    // Intentionally pass raw data as the API does not require trailing \0.
    if (auto doc = xmlReadMemory(reinterpret_cast<const char*>(data), size,
                                 "noname.xml", NULL, option_value)) {
      auto buffer = xmlBufferCreate();
      assert(buffer);

      auto context = xmlSaveToBuffer(buffer, NULL, 0);
      xmlSaveDoc(context, doc);
      xmlSaveClose(context);
      xmlFreeDoc(doc);
      xmlBufferFree(buffer);
    }
  }

  return 0;
}
