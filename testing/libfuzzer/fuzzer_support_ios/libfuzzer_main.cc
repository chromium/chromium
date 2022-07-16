// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "testing/libfuzzer/fuzzer_support_ios/fuzzer_support.h"

int main(int argc, char** argv) {
  ios_fuzzer::RunFuzzerFromIOSApp(argc, argv);
  return 0;
}
