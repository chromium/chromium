// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_TESTS_BASIC_PROCESS_SHIM_DWRITE_DECLARATIONS_H_
#define SANDBOX_WIN_TESTS_BASIC_PROCESS_SHIM_DWRITE_DECLARATIONS_H_

#include <dwrite.h>

extern "C" {

HRESULT WINAPI DWriteCoreCreateFactory(DWRITE_FACTORY_TYPE factoryType,
                                       REFIID iid,
                                       IUnknown** factory);

}  // extern "C"

#endif  // SANDBOX_WIN_TESTS_BASIC_PROCESS_SHIM_DWRITE_DECLARATIONS_H_
