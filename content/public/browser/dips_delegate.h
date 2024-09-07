// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DIPS_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DIPS_DELEGATE_H_

#include "content/common/content_export.h"

namespace content {

class BrowserContext;

// DipsDelegate is an interface that lets the //content layer
// provide embedder specific configuration for DIPS (Bounce Tracking
// Mitigations).
//
// Instances can be obtained via
// ContentBrowserClient::CreateDipsDelegate().
class CONTENT_EXPORT DipsDelegate {
 public:
  virtual ~DipsDelegate();

  // DIPS will be enabled in browser contexts for which this returns true.
  virtual bool ShouldEnableDips(BrowserContext* browser_context) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DIPS_DELEGATE_H_
