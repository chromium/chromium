// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_TEST_EXTENSION_DIR_H_
#define EXTENSIONS_TEST_TEST_EXTENSION_DIR_H_

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_piece.h"

namespace extensions {

// Provides a temporary directory to build an extension into.  This lets all of
// an extension's code live inside the test instead of in a separate directory.
class TestExtensionDir {
 public:
  TestExtensionDir();

  ~TestExtensionDir();

  // Writes |manifest| to manifest.json within the unpacked dir.  No validation
  // is performed. If desired this should be done on extension installation.
  void WriteManifest(base::StringPiece manifest);

  // Like WriteManifest, but where the |manifest| is given in single-quotes
  // rather than double-quotes. This is for convenience to avoid escaping.
  //
  // E.g. |manifest| can be {'name': 'me'} rather than {\"name\": \"me\"}.
  void WriteManifestWithSingleQuotes(base::StringPiece manifest);

  // Writes |contents| to |filename| within the unpacked dir, overwriting
  // anything that was already there.
  void WriteFile(const base::FilePath::StringType& filename,
                 base::StringPiece contents);

  // Packs the extension into a .crx, and returns the path to that
  // .crx. Multiple calls to Pack() will produce extensions with the same ID.
  base::FilePath Pack();

  // Returns the path to the unpacked directory.
  base::FilePath UnpackedPath() const;

 private:
  // Stores files that make up the extension.
  base::ScopedTempDir dir_;

  // Stores the generated .crx and .pem.
  base::ScopedTempDir crx_dir_;
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_TEST_EXTENSION_DIR_H_
