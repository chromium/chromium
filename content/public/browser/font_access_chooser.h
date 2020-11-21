// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FONT_ACCESS_CHOOSER_H_
#define CONTENT_PUBLIC_BROWSER_FONT_ACCESS_CHOOSER_H_

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/font_access/font_access.mojom.h"

namespace content {

class CONTENT_EXPORT FontAccessChooser {
 public:
  using Callback =
      base::OnceCallback<void(blink::mojom::FontEnumerationStatus,
                              std::vector<blink::mojom::FontMetadataPtr>)>;
  FontAccessChooser() = default;
  virtual ~FontAccessChooser() = default;

  FontAccessChooser(const FontAccessChooser&) = delete;
  FontAccessChooser operator=(const FontAccessChooser&) = delete;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FONT_ACCESS_CHOOSER_H_
