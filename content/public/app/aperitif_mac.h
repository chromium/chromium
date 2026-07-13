// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_APERITIF_MAC_H_
#define CONTENT_PUBLIC_APP_APERITIF_MAC_H_

// The macOS //content Apéritif dynamic library. The main and helper
// executables, compiled from main_mac.c, are plain C files that do not directly
// link to any C++ targets nor system libraries. Not linking to system libraries
// is important for sandboxing (see //sandbox/mac/seatbelt_sandbox_design.md),
// and not using C++ keeps the executable size to a minimum, because the project
// uses its own version of libc++. Minimizing the size of the executables is
// important because //content embedders need to distribute several duplicate
// helper executables that each have distinct codesigning entitlements (see
// //content/public/app/mac_helpers.gni).
//
// Apéritif is a dynamic library that contains C++ functionality that needs to
// run in the executable prior to all other code. The primary purpose is to
// engage the sandbox in helper executables prior to any system library static
// initializers running.
//
// Apéritif's core functionality executes as a C++ static initializer. Linking
// to this library has side-effects, such as enabling the sandbox.

#ifdef __cplusplus
extern "C" {
#endif

// Asserts that Apéritif has been properly initialized. If Apéritif has not
// finished initializing, this function terminates the process with a fatal
// error.
__attribute__((visibility("default"))) void AperitifCheckInitialized();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // CONTENT_PUBLIC_APP_APERITIF_MAC_H_
