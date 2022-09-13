// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_IDENTIFIABILITY_METRICS_H_
#define EXTENSIONS_COMMON_IDENTIFIABILITY_METRICS_H_

#include "extensions/common/extension_id.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

class GURL;

namespace extensions {

// Encodes |type| and |extension_id| as an identifiability surface.
blink::IdentifiableSurface SurfaceForExtension(
    blink::IdentifiableSurface::Type type,
    const ExtensionId& extension_id);

// Used for histograms. Do not reorder.
enum class ExtensionResourceAccessResult : int {
  kSuccess,
  kCancel,   // Only logged on navigation when the navigation is cancelled and
             // the document stays in place.
  kFailure,  // resource load failed or navigation to some sort of error page.
};

// Records results of attempts to access an extension resource at the url
// |gurl|. Done as part of a study to see if this is being used as a
// fingerprinting method.
void RecordExtensionResourceAccessResult(ukm::SourceIdObj ukm_source_id,
                                         const GURL& gurl,
                                         ExtensionResourceAccessResult result);

// Records that the extension |extension_id| has injected a content script into
// page identified by |ukm_source_id|.
void RecordContentScriptInjection(ukm::SourceIdObj ukm_source_id,
                                  const ExtensionId& extension_id);

// Records that the extension |extension_id| has blocked a network request on
// page identified by |ukm_source_id|.
void RecordNetworkRequestBlocked(ukm::SourceIdObj ukm_source_id,
                                 const ExtensionId& extension_id);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_IDENTIFIABILITY_METRICS_H_
