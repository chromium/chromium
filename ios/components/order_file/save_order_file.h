// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_ORDER_FILE_SAVE_ORDER_FILE_H_
#define IOS_COMPONENTS_ORDER_FILE_SAVE_ORDER_FILE_H_

extern "C" {

// Saves procedure calls collected by the clang sanitizer.
// Requires the addition of the --copt=-fsanitize-coverage=func,trace-pc-guard
// build flag.
// See go/ios-order-files for more information.
// Clang sanitizer documentation:
// https://clang.llvm.org/docs/SanitizerCoverage.html
void CRWSaveOrderFile(void);

}  // extern "C"

#endif  // IOS_COMPONENTS_ORDER_FILE_SAVE_ORDER_FILE_H_
