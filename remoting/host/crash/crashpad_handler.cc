// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_refptr.h"
#include "components/stability_report/user_stream_data_source_posix.h"
#include "third_party/crashpad/crashpad/handler/handler_main.h"
#include "third_party/crashpad/crashpad/handler/user_stream_data_source.h"

int main(int argc, char* argv[]) {
  crashpad::UserStreamDataSources user_stream_data_sources;
  user_stream_data_sources.push_back(
      std::make_unique<stability_report::UserStreamDataSourcePosix>());

  return crashpad::HandlerMain(argc, argv, &user_stream_data_sources);
}
