// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_DOCUMENT_MANAGER_H_
#define CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_DOCUMENT_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/browser/speculation_rules/prefetch/prefetch_type.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"
#include "url/gurl.h"

namespace content {

class NavigationHandle;
class PrefetchContainer;
class PrefetchService;

// Manages the state of and tracks metrics about prefetches for a single page
// load.
class CONTENT_EXPORT PrefetchDocumentManager
    : public DocumentUserData<PrefetchDocumentManager>,
      public WebContentsObserver {
 public:
  ~PrefetchDocumentManager() override;

  PrefetchDocumentManager(const PrefetchDocumentManager&) = delete;
  const PrefetchDocumentManager operator=(const PrefetchDocumentManager&) =
      delete;

  // WebContentsObserver.
  void DidStartNavigation(NavigationHandle* navigation_handle) override;

  // Processes the given speculation candidates to see if they can be
  // prefetched. Any candidates that can be prefetched are removed from
  // |candidates|, and a prefetch for the URL of the candidate is started.
  void ProcessCandidates(
      std::vector<blink::mojom::SpeculationCandidatePtr>& candidates);

  // Starts the process to prefetch |url| with the given |prefetch_type|.
  void PrefetchUrl(const GURL& url, const PrefetchType& prefetch_type);

  // Releases ownership of the |PrefetchContainer| associated with |url|. The
  // prefetch is removed from |owned_prefetches_|, but a pointer to it remains
  // in |all_prefetches_|.
  std::unique_ptr<PrefetchContainer> ReleasePrefetchContainer(const GURL& url);

  static void SetPrefetchServiceForTesting(PrefetchService* prefetch_service);

 private:
  explicit PrefetchDocumentManager(RenderFrameHost* rfh);
  friend DocumentUserData;

  // Helper function to get the |PrefetchService| associated with |this|.
  PrefetchService* GetPrefetchService() const;

  // This map holds references to all |PrefetchContainer| associated with
  // |this|, regardless of ownership.
  std::map<GURL, base::WeakPtr<PrefetchContainer>> all_prefetches_;

  // This map holds all |PrefetchContainer| currently owned by |this|. |this|
  // owns all |PrefetchContainer| from when they are created in |PrefetchUrl|
  // until |PrefetchService| starts the network request for the prefetch, at
  // which point |PrefetchService| takes ownership.
  std::map<GURL, std::unique_ptr<PrefetchContainer>> owned_prefetches_;

  base::WeakPtrFactory<PrefetchDocumentManager> weak_method_factory_{this};

  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_DOCUMENT_MANAGER_H_
