// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SECURE_EMBED_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_SECURE_EMBED_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents.h"

namespace content {

// Implementations of this class are passed to WebContents that are going to be
// embedded via SecureEmebed, to help them communicate with their embedder.
//
// TODO(secure-embed): If this remains this simple, we may want to replace it
// with just passing a WebContents*.
class CONTENT_EXPORT SecureEmbedDelegate {
 public:
  virtual ~SecureEmbedDelegate() = default;

  // Returns the WebContents that currently owns this guest.
  virtual WebContents* GetEmbedderWebContents() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SECURE_EMBED_DELEGATE_H_
