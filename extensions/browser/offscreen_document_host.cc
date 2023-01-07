// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/offscreen_document_host.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

OffscreenDocumentHost::OffscreenDocumentHost(
    const Extension& extension,
    content::SiteInstance* site_instance,
    const GURL& url)
    : ExtensionHost(&extension,
                    site_instance,
                    url,
                    mojom::ViewType::kOffscreenDocument) {
  DCHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionsOffscreenDocuments));
  DCHECK_EQ(url::Origin::Create(url), extension.origin());
  DCHECK_GE(extension.manifest_version(), 3);
}

OffscreenDocumentHost::~OffscreenDocumentHost() = default;

void OffscreenDocumentHost::OnDidStopFirstLoad() {
  // Nothing to do for offscreen documents.
}

bool OffscreenDocumentHost::IsBackgroundPage() const {
  return false;
}

}  // namespace extensions
