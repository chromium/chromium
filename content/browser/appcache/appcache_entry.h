// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_ENTRY_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_ENTRY_H_

#include <stdint.h>

#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"

namespace content {

// A cached entry is identified by a URL and is classified into one
// (or more) categories.  URL is not stored here as this class is stored
// with the URL as a map key or the user of this class already knows the URL.
class AppCacheEntry {
 public:

  // An entry can be of more than one type so use a bitmask.
  // Note: These bit values are stored on disk.
  enum Type {
    MASTER = 1 << 0,
    MANIFEST = 1 << 1,
    EXPLICIT = 1 << 2,
    FOREIGN = 1 << 3,
    FALLBACK = 1 << 4,
    INTERCEPT = 1 << 5,
  };

  AppCacheEntry()
      : types_(0),
        response_id_(blink::mojom::kAppCacheNoResponseId),
        response_size_(0),
        padding_size_(0) {}

  explicit AppCacheEntry(int type)
      : types_(type),
        response_id_(blink::mojom::kAppCacheNoResponseId),
        response_size_(0),
        padding_size_(0) {}

  AppCacheEntry(int type, int64_t response_id)
      : types_(type),
        response_id_(response_id),
        response_size_(0),
        padding_size_(0) {}

  AppCacheEntry(int type,
                int64_t response_id,
                int64_t response_size,
                int64_t padding_size)
      : types_(type),
        response_id_(response_id),
        response_size_(response_size),
        padding_size_(padding_size) {
    DCHECK_GE(response_size, 0);
    DCHECK_GE(padding_size, 0);

    // The manifest determines the origin of the cache, so it should always be a
    // same-origin entry, which should have 0 padding.
    DCHECK((type & MANIFEST) == 0 || padding_size_ == 0);

    // Documents aren't allowed to reference cross-origin manifests, so master
    // entries should always be same-origin, with 0 padding.
    DCHECK((type & MASTER) == 0 || padding_size_ == 0);
  }

  int types() const { return types_; }
  void add_types(int added_types) {
    DCHECK((added_types & MANIFEST) == 0 || padding_size_ == 0);
    DCHECK((added_types & MASTER) == 0 || padding_size_ == 0);
    types_ |= added_types;
  }
  bool IsMaster() const { return (types_ & MASTER) != 0; }
  bool IsManifest() const { return (types_ & MANIFEST) != 0; }
  bool IsExplicit() const { return (types_ & EXPLICIT) != 0; }
  bool IsForeign() const { return (types_ & FOREIGN) != 0; }
  bool IsFallback() const { return (types_ & FALLBACK) != 0; }
  bool IsIntercept() const { return (types_ & INTERCEPT) != 0; }

  int64_t response_id() const { return response_id_; }
  void set_response_id(int64_t id) { response_id_ = id; }
  bool has_response_id() const {
    return response_id_ != blink::mojom::kAppCacheNoResponseId;
  }

  // The actual size of the response data written to disk.
  int64_t response_size() const { return response_size_; }

  // The amount of space added to the actual size when calculating quota usage.
  //
  // To avoid leaking cross-origin resource sizes, the quota system pretends
  // that cross-origin resources are larger than they actually are. The padding
  // is only used for quota accounting, and does not consume any space on the
  // user's disk.
  int64_t padding_size() const { return padding_size_; }

  void SetResponseAndPaddingSizes(int64_t response_size, int64_t padding_size) {
    DCHECK_GE(response_size, 0);
    DCHECK_GE(padding_size, 0);
    DCHECK((types_ & MANIFEST) == 0 || padding_size == 0);
    DCHECK((types_ & MASTER) == 0 || padding_size == 0);
    response_size_ = response_size;
    padding_size_ = padding_size;
  }

 private:
  int types_;
  int64_t response_id_;
  int64_t response_size_;
  int64_t padding_size_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_ENTRY_H_
