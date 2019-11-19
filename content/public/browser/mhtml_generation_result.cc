// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/mhtml_generation_result.h"

namespace content {

MHTMLGenerationResult::MHTMLGenerationResult(int64_t file_size,
                                             const std::string* digest)
    : file_size(file_size) {
  if (digest) {
    file_digest = base::Optional<std::string>(*digest);
  } else {
    file_digest = base::nullopt;
  }
}

MHTMLGenerationResult::MHTMLGenerationResult(
    const MHTMLGenerationResult& other) = default;

MHTMLGenerationResult::~MHTMLGenerationResult() = default;

}  // namespace content
