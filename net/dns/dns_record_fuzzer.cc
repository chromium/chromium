// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "net/dns/dns_response.h"
#include "net/dns/record_parsed.h"

void InitLogging() {
  // For debugging, it may be helpful to enable verbose logging by setting the
  // minimum log level to (-LOG_FATAL).
  logging::SetMinLogLevel(logging::LOG_FATAL);

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  settings.log_file_path = nullptr;
  logging::InitLogging(settings);
}

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  InitLogging();

  net::DnsRecordParser parser(data, size, 0);
  if (!parser.IsValid()) {
    return 0;
  }

  base::Time time;
  std::unique_ptr<const net::RecordParsed> record_parsed;
  do {
    record_parsed = net::RecordParsed::CreateFrom(&parser, time);
  } while (record_parsed);

  net::DnsResourceRecord record;
  while (parser.ReadRecord(&record)) {
  }

  return 0;
}
