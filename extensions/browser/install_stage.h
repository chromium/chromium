// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_INSTALL_STAGE_H_
#define EXTENSIONS_BROWSER_INSTALL_STAGE_H_

namespace extensions {

// The different stages of the extension installation process.
enum class InstallationStage {
  // The validation of signature of the extensions is about to be started.
  kVerification = 0,
  // Extension archive is about to be copied into the working directory.
  kCopying = 1,
  // Extension archive is about to be unpacked.
  kUnpacking = 2,
  // Performing the expectation checks before the installation can be started.
  kCheckingExpectations = 3,
  // Installation of unpacked extension is started.
  kFinalizing = 4,
  // Extension installation process is complete.
  kComplete = 5,
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_INSTALL_STAGE_H_
