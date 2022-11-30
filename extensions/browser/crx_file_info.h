// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CRX_FILE_INFO_H_
#define EXTENSIONS_BROWSER_CRX_FILE_INFO_H_

#include <string>

#include "base/files/file_path.h"
#include "base/version.h"
#include "extensions/common/extension_id.h"

namespace crx_file {
enum class VerifierFormat;
}

namespace extensions {

// CRXFileInfo holds general information about a cached CRX file
struct CRXFileInfo {
  CRXFileInfo();
  CRXFileInfo(const base::FilePath& path,
              const crx_file::VerifierFormat required_format);
  CRXFileInfo(const CRXFileInfo&);
  ~CRXFileInfo();

  bool operator==(const CRXFileInfo& that) const;

  // Only |path| and |required_format| are mandatory. |extension_id|,
  // |expected_hash| and |expected_version| are only checked if non-empty.
  base::FilePath path;
  crx_file::VerifierFormat required_format;
  ExtensionId extension_id;
  std::string expected_hash;
  base::Version expected_version;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CRX_FILE_INFO_H_
