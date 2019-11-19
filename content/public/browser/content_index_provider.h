// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CONTENT_INDEX_PROVIDER_H_
#define CONTENT_PUBLIC_BROWSER_CONTENT_INDEX_PROVIDER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/content_index/content_index.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

struct CONTENT_EXPORT ContentIndexEntry {
  ContentIndexEntry(int64_t service_worker_registration_id,
                    blink::mojom::ContentDescriptionPtr description,
                    const GURL& launch_url,
                    base::Time registration_time);
  ContentIndexEntry(ContentIndexEntry&& other);
  ContentIndexEntry& operator=(ContentIndexEntry&& other);
  ~ContentIndexEntry();

  // Part of the key for an entry since different service workers can use the
  // same ID.
  int64_t service_worker_registration_id;

  // All the developer provided information.
  blink::mojom::ContentDescriptionPtr description;

  // The fully-resolved URL of the content.
  GURL launch_url;

  // The time the registration was created.
  base::Time registration_time;
};

// Interface for content providers to receive content-related updates.
class CONTENT_EXPORT ContentIndexProvider {
 public:
  ContentIndexProvider();
  virtual ~ContentIndexProvider();

  // Returns the number of icons needed and their ideal sizes (in pixels).
  virtual std::vector<gfx::Size> GetIconSizes(
      blink::mojom::ContentCategory category) = 0;

  // Called when a new entry is registered. Must be called on the UI thread.
  virtual void OnContentAdded(ContentIndexEntry entry) = 0;

  // Called when an entry is unregistered. Must be called on the UI thread.
  virtual void OnContentDeleted(int64_t service_worker_registration_id,
                                const url::Origin& origin,
                                const std::string& description_id) = 0;

  DISALLOW_COPY_AND_ASSIGN(ContentIndexProvider);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONTENT_INDEX_PROVIDER_H_
