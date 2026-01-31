// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PAGE_MANIFEST_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_PAGE_MANIFEST_MANAGER_H_

#include "base/callback_list.h"
#include "base/types/expected.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom-forward.h"

namespace content {
class Page;

// This class returns the existing or next developer-specified manifest on a
// given page to subscribers, without subscribers needing to worry about waiting
// for manifest URLs being specified etc.
//
// It is intended that the manifest here is cached and returned, no matter what
// lifecycle stage the page is in (can help with doing work during prerendering,
// or access the manifest after a page becomes primary after being in the
// bfcache). Callers that also want to make sure the page is primary, active,
// and not in a fenced frame, need to do do so separately from calling this API.
class CONTENT_EXPORT PageManifestManager {
 public:
  using ManifestResult = base::expected<blink::mojom::ManifestPtr,
                                        blink::mojom::RequestManifestErrorPtr>;
  using ManifestCallbackList =
      base::OnceCallbackList<void(const ManifestResult& manifest_result)>;

  using AllManifestsCallbackList =
      base::RepeatingCallbackList<void(const ManifestResult& manifest_result)>;

  static PageManifestManager* GetOrCreate(Page& page);

  virtual ~PageManifestManager() = default;

  // Returns the current or next developer-specified manifest on this page
  // (where the developer specifies a manifest link). The callback is guaranteed
  // to be called asynchronously.
  //
  // The `callback` is called with a `blink::mojom::ManifestPtr` on success. On
  // failure (e.g. the manifest URL fails to load, or the manifest fails to
  // parse), it is called with a `blink::mojom::RequestManifestErrorPtr`.
  virtual base::CallbackListSubscription GetSpecifiedManifest(
      ManifestCallbackList::CallbackType callback) = 0;

  // Subscribes to every developer-specified manifest on this page.
  //
  // The provided `callback` is called SYNCHRONOUSLY for the
  // currently specified manifest (if any) when this is first called, and
  // ASYNCHRONOUSLY for subsequent manifests found when the  manifest URL
  // changes on the page.
  //
  // A `callback` is called with a `blink::mojom::ManifestPtr` on success. On
  // failure (e.g. the manifest URL fails to load, or the manifest fails to
  // parse), it is called with a `blink::mojom::RequestManifestErrorPtr`.
  //
  // The `callback` is NOT called when the developer removes the manifest URL
  // from the page (i.e. changing from a specified manifest to no manifest).
  virtual base::CallbackListSubscription GetAllSpecifiedManifests(
      AllManifestsCallbackList::CallbackType callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PAGE_MANIFEST_MANAGER_H_
