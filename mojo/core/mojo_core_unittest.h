// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_MOJO_CORE_UNITTEST_H_
#define MOJO_CORE_MOJO_CORE_UNITTEST_H_

namespace switches {

// Instructs the test runner to initialize the Mojo Core library in separate
// phases. Used by a unit test when launching a subprocess, to verify the
// correctness of phased initialization.
extern const char kMojoLoadBeforeInit[];

// Instructs the test runner to provide an explicit path to the Mojo Core shared
// library, rather than assuming it's present in the current working directory.
extern const char kMojoUseExplicitLibraryPath[];

}  // namespace switches

#endif  // MOJO_CORE_MOJO_CORE_UNITTEST_H_
