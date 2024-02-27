// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DIPS_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DIPS_DELEGATE_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
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
  using EngagedSitesCallback =
      base::OnceCallback<void(std::vector<std::string>)>;

  virtual ~DipsDelegate();

  // On the first startup, GetEngagedSites() will be called and the DIPS
  // Database will be prepopulated with the sites passed to `callback`.
  virtual void GetEngagedSites(BrowserContext* browser_context,
                               EngagedSitesCallback callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DIPS_DELEGATE_H_
