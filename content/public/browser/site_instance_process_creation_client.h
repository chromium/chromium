// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SITE_INSTANCE_PROCESS_CREATION_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_SITE_INSTANCE_PROCESS_CREATION_CLIENT_H_

#include "base/types/pass_key.h"
#include "content/common/content_export.h"

namespace chromecast {
class RendererPrelauncher;
}  // namespace chromecast

namespace content {

// A static class that provides access to SiteInstance::GetOrCreateProcess()
// outside //content. The API explicitly creates a renderer process for a
// SiteInstance and is discouraged to be used. The API is exported to the
// chromecast RendererPrelauncher for migration purpose.
// TODO(crbug.com/424051832): Migrate RendererPrelauncher to use the spare
// renderer and remove the class.
class CONTENT_EXPORT SiteInstanceProcessCreationClient {
 public:
  using PassKey = base::PassKey<SiteInstanceProcessCreationClient>;

  SiteInstanceProcessCreationClient() = delete;

 private:
  friend class chromecast::RendererPrelauncher;

  static PassKey GetPassKey();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SITE_INSTANCE_PROCESS_CREATION_CLIENT_H_
