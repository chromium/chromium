// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_ZUCCHINI_H_
#define COMPONENTS_ZUCCHINI_ZUCCHINI_H_

#include <string>

#include "components/zucchini/buffer_view.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/patch_reader.h"
#include "components/zucchini/patch_writer.h"

// Core Zucchini library, consisting of:
// - Global constants.
// - Patch gen and apply functions, where "old" and "new" data are represented
//   as buffers, and patch data represented as EnsemblePatchWriter or
//   EnsemblePatchReader.

namespace zucchini {

// Zucchini-gen options with default values.
struct GenerateOptions {
  // The file offset to start element detection. This is a kludge to handle
  // self-extracting archives that embeds multiple executables in its data
  // section: In this case it's better to skip the first few bytes to avoid
  // detecting the "outer" binary, thereby avoiding trating the "inner" binaries
  // as raw data.
  offset_t start_scan_at = 0U;

  // Imposed match: Encoded string that encodes how to override default element
  // detection and matching heuristics with custom element matching. The string
  // is formatted as:
  //   "#+#=#+#,#+#=#+#,..."  (e.g., "1+2=3+4", "1+2=3+4,5+6=7+8"),
  // where "#+#=#+#" encodes a match as 4 unsigned integers:
  //   [offset in "old", size in "old", offset in "new", size in "new"].
  // If empty then use default element detection and matching.
  std::string imposed_matches;

  // Whether to generate raw patch, i.e., do not scan for elements.
  bool is_raw = false;
};

namespace status {

// Zucchini status code, which can also be used as process exit code. Therefore
// success is explicitly 0.
// LINT.IfChange
enum Code {
  kStatusSuccess = 0,
  kStatusInvalidParam = 1,
  kStatusFileReadError = 2,
  kStatusFileWriteError = 3,
  kStatusPatchReadError = 4,
  kStatusPatchWriteError = 5,
  kStatusInvalidOldImage = 6,
  kStatusInvalidNewImage = 7,
  kStatusDiskFull = 8,
  kStatusIoError = 9,
  kStatusFatal = 10,
};
// LINT.ThenChange(/components/services/patch/public/mojom/file_patcher.mojom)

}  // namespace status

// Generates ensemble patch from |old_image| to |new_image| using the
// specified |options|, writes the results to |patch_writer|, and returns a
// status::Code.
status::Code GenerateBuffer(ConstBufferView old_image,
                            ConstBufferView new_image,
                            const GenerateOptions& options,
                            EnsemblePatchWriter* patch_writer);

// Deprecated: Interface to specify default GenerateOptions.
status::Code GenerateBuffer(ConstBufferView old_image,
                            ConstBufferView new_image,
                            EnsemblePatchWriter* patch_writer);

// Deprecated: Interface to specify GenerateOptions{.imposed_matches}.
status::Code GenerateBufferImposed(ConstBufferView old_image,
                                   ConstBufferView new_image,
                                   std::string imposed_matches,
                                   EnsemblePatchWriter* patch_writer);

// Deprecated: Interface to specify GenerateOptions{.is_raw = true}.
status::Code GenerateBufferRaw(ConstBufferView old_image,
                               ConstBufferView new_image,
                               EnsemblePatchWriter* patch_writer);

// Applies |patch_reader| to |old_image| to build |new_image|, which refers to
// preallocated memory of sufficient size.
status::Code ApplyBuffer(ConstBufferView old_image,
                         const EnsemblePatchReader& patch_reader,
                         MutableBufferView new_image);

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_ZUCCHINI_H_
