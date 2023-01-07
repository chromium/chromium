// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_ZUCCHINI_INTEGRATION_H_
#define COMPONENTS_ZUCCHINI_ZUCCHINI_INTEGRATION_H_

#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "components/zucchini/zucchini.h"

// Zucchini integration interface to wrap core Zucchini library with file I/O.

namespace zucchini {

// Generates a patch to transform |old_file| to |new_file|, and writes the
// result to |patch_file|. Since this uses memory mapped files, crashes are
// expected in case of I/O errors. On Windows, |patch_file| is kept iff returned
// code is kStatusSuccess or if |force_keep == true|, and is deleted otherwise.
// For UNIX systems the caller needs to do cleanup since it has ownership of the
// base::File params, and Zucchini has no knowledge of which base::FilePath to
// delete. If |is_raw == true| then uses Raw Zucchini. If |imposed_matches| is
// non-empty, then overrides default element detection and matching heuristics
// with custom element matching encoded in |imposed_matches|, which should be
// formatted as:
//   "#+#=#+#,#+#=#+#,..."  (e.g., "1+2=3+4", "1+2=3+4,5+6=7+8"),
// where "#+#=#+#" encodes a match as 4 unsigned integers:
//   [offset in "old", size in "old", offset in "new", size in "new"].
status::Code Generate(base::File old_file,
                      base::File new_file,
                      base::File patch_file,
                      bool force_keep = false,
                      bool is_raw = false,
                      std::string imposed_matches = "");

// Alternative Generate() interface that takes base::FilePath as arguments.
// Performs proper cleanup in Windows and UNIX if failure occurs.
status::Code Generate(const base::FilePath& old_path,
                      const base::FilePath& new_path,
                      const base::FilePath& patch_path,
                      bool force_keep = false,
                      bool is_raw = false,
                      std::string imposed_matches = "");

// Applies the patch in |patch_file| to |old_file|, and writes the result to
// |new_file|. Since this uses memory mapped files, crashes are expected in case
// of I/O errors. On Windows, |new_file| is kept iff returned code is
// kStatusSuccess or if |force_keep == true|, and is deleted otherwise. For UNIX
// systems the caller needs to do cleanup since it has ownership of the
// base::File params, and Zucchini has no knowledge of which base::FilePath to
// delete.
status::Code Apply(base::File old_file,
                   base::File patch_file,
                   base::File new_file,
                   bool force_keep = false);

// Alternative Apply() interface that takes base::FilePath as arguments.
// Performs proper cleanup in Windows and UNIX if failure occurs.
status::Code Apply(const base::FilePath& old_path,
                   const base::FilePath& patch_path,
                   const base::FilePath& new_path,
                   bool force_keep = false);

// Verifies the patch format in |patch_file| and returns
// Code::kStatusPatchReadError if the patch is malformed or version is
// unsupported. Since this uses memory mapped files, crashes are expected in
// case of I/O errors.
status::Code VerifyPatch(base::File patch_file);

// Alternative VerifyPatch() interface that takes base::FilePath as arguments.
// Performs proper cleanup in Windows and UNIX if failure occurs.
status::Code VerifyPatch(const base::FilePath& patch_path);

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_ZUCCHINI_INTEGRATION_H_
