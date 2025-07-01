// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/perf_test_suite.h"
#include "mojo/core/embedder/embedder.h"

int main(int argc, char** argv) {
  mojo::core::Init();
  return base::PerfTestSuite(argc, argv).Run();
}

