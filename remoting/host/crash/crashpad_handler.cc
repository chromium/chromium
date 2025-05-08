// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/crashpad/crashpad/handler/handler_main.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <stdlib.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
#endif

#if BUILDFLAG(IS_LINUX)

int main(int argc, char* argv[]) {
  return crashpad::HandlerMain(argc, argv,
                               /*user_stream_data_sources=*/nullptr);
}

#elif BUILDFLAG(IS_WIN)

namespace {

int HandlerMainAdaptor(int argc, char* argv[]) {
  return crashpad::HandlerMain(argc, argv,
                               /*user_stream_data_sources=*/nullptr);
}

}  // namespace

int wmain(int argc, wchar_t* argv[]) {
  // Convert wide strings to skinny strings.
  auto argv_as_utf8 = base::HeapArray<char*>::Uninit(argc + 1);
  std::vector<std::string> storage;
  storage.reserve(argc);
  auto argv_span = UNSAFE_BUFFERS(base::span<wchar_t*>(
      argv, static_cast<size_t>(argc)));  // SAFETY: argv,argc come from os.
  for (int i = 0; i < argc; ++i) {
    storage.push_back(base::WideToUTF8(argv_span[i]));
    argv_as_utf8[i] = &storage[i][0];
  }
  argv_as_utf8[argc] = nullptr;
  return HandlerMainAdaptor(argc, argv_as_utf8.data());
}

#endif  // BUILDFLAG(IS_LINUX)
