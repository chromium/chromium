// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_DOCUMENT_MANAGER_H_
#define CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_DOCUMENT_MANAGER_H_

#include <vector>

#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"
#include "url/gurl.h"

namespace content {

// Manages the state of and tracks metrics about prefetches for a single page
// load.
class CONTENT_EXPORT PrefetchDocumentManager
    : public DocumentUserData<PrefetchDocumentManager> {
 public:
  ~PrefetchDocumentManager() override;

  PrefetchDocumentManager(const PrefetchDocumentManager&) = delete;
  const PrefetchDocumentManager operator=(const PrefetchDocumentManager&) =
      delete;

  void ProcessCandidates(
      std::vector<blink::mojom::SpeculationCandidatePtr>& candidates);

  void PrefetchUrl(const GURL& url);

 private:
  explicit PrefetchDocumentManager(RenderFrameHost* rfh);
  friend DocumentUserData;

  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_DOCUMENT_MANAGER_H_
