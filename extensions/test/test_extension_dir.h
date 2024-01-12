// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_TEST_EXTENSION_DIR_H_
#define EXTENSIONS_TEST_TEST_EXTENSION_DIR_H_

#include <string_view>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/values.h"

namespace extensions {

// Provides a temporary directory to build an extension into.  This lets all of
// an extension's code live inside the test instead of in a separate directory.
class TestExtensionDir {
 public:
  TestExtensionDir();

  ~TestExtensionDir();

  TestExtensionDir(const TestExtensionDir&) = delete;
  TestExtensionDir(TestExtensionDir&&) noexcept;

  TestExtensionDir& operator=(const TestExtensionDir&) = delete;
  TestExtensionDir& operator=(TestExtensionDir&&);

  // Writes |manifest| to manifest.json within the unpacked dir. No validation
  // is performed. If desired this should be done on extension installation.
  void WriteManifest(std::string_view manifest);

  // As above, but using a base::Value::Dict instead of JSON string.
  void WriteManifest(const base::Value::Dict& manifest);

  // Writes |contents| to |filename| within the unpacked dir, overwriting
  // anything that was already there.
  void WriteFile(const base::FilePath::StringType& filename,
                 std::string_view contents);

  // Copies the file at |from_path| into |local_filename| under the temp
  // directory, overwriting anything that was already there.
  void CopyFileTo(const base::FilePath& from_path,
                  const base::FilePath::StringType& local_filename);

  // Packs the extension into a .crx, and returns the path to that
  // .crx. Multiple calls to Pack() will produce extensions with the same ID.
  // If `custom_path` is provided, this will create a new .crx (nested under
  // this directory's temp dir) at `custom_path`. This allows for creating
  // multiple versions of a CRX easily without overwriting or moving them. If
  // omitted, the .crx will be at "ext.crx".
  base::FilePath Pack(std::string_view custom_path = std::string_view());

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
