// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_APERITIF_MAC_H_
#define CONTENT_PUBLIC_APP_APERITIF_MAC_H_

// The macOS //content Apéritif dynamic library. The main and helper
// executables, compiled from *main_mac.c are plain C files that do not
// directly link to any C++ targets nor system libraries. Not linking to system
// libraries is important for sandboxing (see
// //sandbox/mac/seatbelt_sandbox_design.md), and not including C++ keeps the
// executable size to a minimum, because the project uses its own version of
// libc++. Minimizing the size of the executables is important because
// //content embedders need to distribute several duplicate helper executables
// that each have distinct codesigning entitlements (see
// //content/public/app/mac_helpers.gni).
//
// Apéritif is a dynamic library that contains C++ functionality that needs to
// run in the executable prior to loading the main Framework. This primary
// purpose is to engage the sandbox in helper executables prior to any system
// library static initializers running.

#define CONTENT_APERITIF_EXPORT __attribute__((visibility("default")))

#ifdef __cplusplus
extern "C" {
#endif

// Helper function to record an error message and abort.
void CONTENT_APERITIF_EXPORT __attribute__((noreturn, format(printf, 1, 2)))
AperitifFatalError(const char* format, ...);

// Performs early initialization of the Mac allocator zone.
void CONTENT_APERITIF_EXPORT AperitifInitializePartitionAlloc();

// Engages the sandbox if the command line arguments specify that the process
// is to be sandboxed.
void CONTENT_APERITIF_EXPORT
AperitifInitializeSandbox(const char* executable_path,
                          int argc,
                          const char* const argv[]);

#ifdef __cplusplus
}  // extern "C"
#endif

#undef CONTENT_APERITIF_EXPORT

#endif  // CONTENT_PUBLIC_APP_APERITIF_MAC_H_
