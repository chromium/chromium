// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_OR_RESOURCE_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_OR_RESOURCE_CONTEXT_H_

#include <cstddef>

#include "base/memory/raw_ref.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

class BrowserContext;
class ResourceContext;

// A class holding either BrowserContext*, a ResourceContext*, or nothing. This
// class should hold a BrowserContext* when constructed on the UI thread and a
// ResourceContext* when constructed on the IO thread. This object must only be
// accessed on the thread it was constructed and does not allow converting
// between the two pointer types.
class CONTENT_EXPORT BrowserOrResourceContext final {
 public:
  BrowserOrResourceContext();
  ~BrowserOrResourceContext();

  BrowserOrResourceContext(const BrowserOrResourceContext&);
  BrowserOrResourceContext& operator=(const BrowserOrResourceContext&);

  // BrowserOrResourceContext is constructible from either BrowserContext* or
  // ResourceContext*.
  // TODO(dcheng): Change this to take a ref.
  explicit BrowserOrResourceContext(BrowserContext* browser_context);

  // TODO(dcheng): Change this to take a ref.
  explicit BrowserOrResourceContext(ResourceContext* resource_context);

  BrowserOrResourceContext(std::nullptr_t) = delete;

  // Returns true if `this` is not null.
  explicit operator bool() const {
    return !absl::holds_alternative<absl::monostate>(storage_);
  }

  // To be called only on the UI thread. Will CHECK() if `this` does not hold a
  // `BrowserContext*`.
  // TODO(dcheng): Change this to return a ref.
  BrowserContext* ToBrowserContext() const {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return &*absl::get<raw_ref<BrowserContext, DanglingUntriaged>>(storage_);
  }

  // To be called only on the UI thread. Will CHECK() if `this` does not hold a
  // `ResourceContext*`.
  // TODO(dcheng): Change this to return a ref.
  ResourceContext* ToResourceContext() const {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    return &*absl::get<raw_ref<ResourceContext, DanglingUntriaged>>(storage_);
  }

 private:
  // `absl::monostate` corresponds to the null state.
  absl::variant<absl::monostate,
                raw_ref<BrowserContext, DanglingUntriaged>,
                raw_ref<ResourceContext, DanglingUntriaged>>
      storage_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_OR_RESOURCE_CONTEXT_H_
