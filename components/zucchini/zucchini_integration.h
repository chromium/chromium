// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_ZUCCHINI_INTEGRATION_H_
#define COMPONENTS_ZUCCHINI_ZUCCHINI_INTEGRATION_H_

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
// delete. |options| contains settings that directly affect patch generation.
status::Code Generate(base::File old_file,
                      base::File new_file,
                      base::File patch_file,
                      const GenerateOptions& options,
                      bool force_keep = false);

// Alternative Generate() interface that takes base::FilePath as arguments.
// Performs proper cleanup in Windows and UNIX if failure occurs.
status::Code Generate(const base::FilePath& old_path,
                      const base::FilePath& new_path,
                      const base::FilePath& patch_path,
                      const GenerateOptions& options,
                      bool force_keep = false);

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
