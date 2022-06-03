// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FONT_ACCESS_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_FONT_ACCESS_DELEGATE_H_

#include "content/common/content_export.h"
#include "content/public/browser/font_access_chooser.h"

namespace content {

class RenderFrameHost;

class CONTENT_EXPORT FontAccessDelegate {
 public:
  virtual ~FontAccessDelegate() = default;

  virtual std::unique_ptr<FontAccessChooser> RunChooser(
      RenderFrameHost* frame,
      const std::vector<std::string>& selection,
      FontAccessChooser::Callback callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FONT_ACCESS_DELEGATE_H_
