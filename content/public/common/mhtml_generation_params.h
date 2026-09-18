// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_MHTML_GENERATION_PARAMS_H_
#define CONTENT_PUBLIC_COMMON_MHTML_GENERATION_PARAMS_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "content/common/content_export.h"

namespace content {

class RenderFrameHost;

struct CONTENT_EXPORT MHTMLGenerationParams {
  using FrameFilterCallback = base::RepeatingCallback<bool(RenderFrameHost*)>;

  explicit MHTMLGenerationParams(const base::FilePath& file_path);
  MHTMLGenerationParams(const MHTMLGenerationParams&);
  MHTMLGenerationParams& operator=(const MHTMLGenerationParams&);
  MHTMLGenerationParams(MHTMLGenerationParams&&);
  MHTMLGenerationParams& operator=(MHTMLGenerationParams&&);
  ~MHTMLGenerationParams();

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

  // An optional callback that can be used to filter which frames are included
  // in the generated MHTML. If set, this callback is invoked for each subframe;
  // returning false skips serializing that frame (and any descendant frames).
  FrameFilterCallback frame_filter;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_MHTML_GENERATION_PARAMS_H_
