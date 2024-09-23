// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MHTML_GENERATION_RESULT_H_
#define CONTENT_PUBLIC_BROWSER_MHTML_GENERATION_RESULT_H_

#include <cstdint>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"

namespace content {

// A result container for the output of executing
// WebContents::GenerateMHTMLWithResult().
struct CONTENT_EXPORT MHTMLGenerationResult {
  // GenerateMHTMLCallback is called to report completion and status of MHTML
  // generation. Expects an MHTMLGenerationResult object.
  using GenerateMHTMLCallback =
      base::OnceCallback<void(const MHTMLGenerationResult& result)>;

  MHTMLGenerationResult(int64_t file_size, const std::string* digest);
  MHTMLGenerationResult(const MHTMLGenerationResult& other);
  ~MHTMLGenerationResult();

  // Size of the generated file. On success |file_size| denotes the size of the
  // generated file. On failure |file_size| is -1.
  int64_t file_size;

  // The SHA-256 digest of the generated file. On success, |file_digest|
  // contains the digest of the generated file, otherwise |file_digest| is
  // std::nullopt.
  std::optional<std::string> file_digest;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MHTML_GENERATION_RESULT_H_
