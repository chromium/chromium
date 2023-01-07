// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_VERIFIER_FORMATS_H_
#define EXTENSIONS_COMMON_VERIFIER_FORMATS_H_

namespace crx_file {
enum class VerifierFormat;
}

namespace extensions {

// Returns the default format requirement for installing an extension that
// originates or updates from the Webstore. |test_publisher_enabled| indicates
// whether items from a test instance of Webstore are permitted.
crx_file::VerifierFormat GetWebstoreVerifierFormat(bool test_publisher_enabled);

// Returns the default format requirement for installing an extension that
// is force-installed by policy.
crx_file::VerifierFormat GetPolicyVerifierFormat();

// Returns the default format requirement for installing an extension that
// is installed from an external source.
crx_file::VerifierFormat GetExternalVerifierFormat();

// Returns the default format requirement for installing an extension that
// is installed in a unit or browser test context.
crx_file::VerifierFormat GetTestVerifierFormat();

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_VERIFIER_FORMATS_H_
