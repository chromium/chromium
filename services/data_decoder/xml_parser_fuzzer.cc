// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "services/data_decoder/xml_parser.h"
#include "third_party/libxml/chromium/libxml_utils.h"

namespace {

void OnParseXml(base::Closure quit_loop,
                base::Optional<base::Value> value,
                const base::Optional<std::string>& error) {
  std::move(quit_loop).Run();
}

// Error handler to ignore spamy messages from libxml.
void ignore(void* ctx, const char* msg, ...) {}

}  // namespace

static ScopedXmlErrorFunc scoped_xml_error_func(nullptr, &ignore);

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const char* data_ptr = reinterpret_cast<const char*>(data);

  data_decoder::XmlParser xml_parser_impl;
  data_decoder::mojom::XmlParser& xml_parser = xml_parser_impl;

  base::SingleThreadTaskExecutor main_thread_task_executor;
  base::RunLoop run_loop;
  xml_parser.Parse(std::string(data_ptr, size),
                   base::Bind(&OnParseXml, run_loop.QuitClosure()));
  run_loop.Run();

  return 0;
}
