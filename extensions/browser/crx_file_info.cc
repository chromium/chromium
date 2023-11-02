// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/crx_file_info.h"

#include "base/check.h"
#include "components/crx_file/crx_verifier.h"

namespace extensions {

CRXFileInfo::CRXFileInfo() : path() {
}

CRXFileInfo::CRXFileInfo(const base::FilePath& p,
                         const crx_file::VerifierFormat f)
    : path(p), required_format(f) {
  DCHECK(!path.empty());
}

CRXFileInfo::CRXFileInfo(const CRXFileInfo& other)
    : path(other.path),
      required_format(other.required_format),
      extension_id(other.extension_id),
      expected_hash(other.expected_hash),
      expected_version(other.expected_version) {
  DCHECK(!path.empty());
}

CRXFileInfo::~CRXFileInfo() = default;

bool CRXFileInfo::operator==(const CRXFileInfo& that) const {
  return path == that.path && required_format == that.required_format &&
         extension_id == that.extension_id &&
         expected_hash == that.expected_hash &&
         expected_version == that.expected_version;
}

}  // namespace extensions
