// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FONT_ACCESS_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_FONT_ACCESS_CONTEXT_H_

#include "base/callback_forward.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/font_access/font_access.mojom.h"

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_MAC) || defined(OS_FUCHSIA)
#define PLATFORM_HAS_LOCAL_FONT_ENUMERATION_IMPL 1
#endif

namespace content {

class CONTENT_EXPORT FontAccessContext {
 public:
  using FindAllFontsCallback =
      base::OnceCallback<void(blink::mojom::FontEnumerationStatus,
                              std::vector<blink::mojom::FontMetadata>)>;
  virtual void FindAllFonts(FindAllFontsCallback callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FONT_ACCESS_CONTEXT_H_
