// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_MHTML_GENERATION_PARAMS_H_
#define CONTENT_PUBLIC_COMMON_MHTML_GENERATION_PARAMS_H_

#include "base/files/file_path.h"
#include "content/common/content_export.h"

namespace content {

struct CONTENT_EXPORT MHTMLGenerationParams {
  MHTMLGenerationParams(const base::FilePath& file_path);
  ~MHTMLGenerationParams() = default;

  // The file that will contain the generated MHTML.
  base::FilePath file_path;

  // If true, a Content-Transfer-Encoding value of 'binary' will be used,
  // instead of a combination of 'quoted-printable' and 'base64'. Binary
  // encoding is known to have interoperability issues and is not the
  // recommended encoding for shareable content. See
  // https://tools.ietf.org/html/rfc2045 for details about
  // Content-Transfer-Encoding.
  bool use_binary_encoding = false;

  // Removes popups that could obstruct the user's view of normal content.
  bool remove_popup_overlay = false;

  // Run page problem detectors while generating MTHML if true.
  bool use_page_problem_detectors = false;

  // Whether to compute the hash of the contents while saving the MHTML file.
  // This is triggered by the feature flag kOnTheFlyMhtmlHashComputation.
  bool compute_contents_hash = false;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_MHTML_GENERATION_PARAMS_H_
