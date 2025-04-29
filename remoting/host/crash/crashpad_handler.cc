// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/crashpad/crashpad/handler/handler_main.h"

int main(int argc, char* argv[]) {
  return crashpad::HandlerMain(argc, argv,
                               /*user_stream_data_sources=*/nullptr);
}
