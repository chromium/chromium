// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SECURE_EMBED_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_SECURE_EMBED_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents.h"

namespace content {

// Implementations of this class are passed to WebContents that are going to be
// embedded via SecureEmbed, to help them communicate with their embedder.
class CONTENT_EXPORT SecureEmbedDelegate {
 public:
  enum class FocusOperation {
    kFocusPlugin,
    kFocusBeforePlugin,
    kFocusAfterPlugin
  };

  virtual ~SecureEmbedDelegate() = default;

  // Returns the WebContents that currently owns this guest.
  virtual WebContents* GetEmbedderWebContents() = 0;

  // Requests focus in the embedder document for either the embedding element,
  // or the elements before or after it in the tab order, based on `focus_op`.
  virtual void FocusInEmbedder(content::WebContents* embedded,
                               FocusOperation focus_op) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SECURE_EMBED_DELEGATE_H_
