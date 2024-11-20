// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_OFFSCREEN_DOCUMENT_HOST_H_
#define EXTENSIONS_BROWSER_OFFSCREEN_DOCUMENT_HOST_H_

#include "extensions/browser/extension_host.h"

class GURL;

namespace content {
class SiteInstance;
}

namespace extensions {
class Extension;

// A host for offscreen extension documents. These are essentially WebContents
// owned by an extension that are rendered offscreen. Unlike background pages,
// these cannot register "lazy" event listeners; they will not be respawned
// in response to API events.
class OffscreenDocumentHost : public ExtensionHost {
 public:
  OffscreenDocumentHost(const Extension& extension,
                        content::SiteInstance* site_instance,
                        const GURL& url);

  OffscreenDocumentHost(const OffscreenDocumentHost&) = delete;
  OffscreenDocumentHost& operator=(const OffscreenDocumentHost&) = delete;

  ~OffscreenDocumentHost() override;

 private:
  // ExtensionHost:
  void OnDidStopFirstLoad() override;
  bool IsBackgroundPage() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_OFFSCREEN_DOCUMENT_HOST_H_
