// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_OR_RESOURCE_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_OR_RESOURCE_CONTEXT_H_

#include <cstddef>
#include <type_traits>

#include "base/check_op.h"
#include "content/public/browser/browser_thread.h"

namespace content {

class BrowserContext;
class ResourceContext;

// A class holding either a BrowserContext* or a ResourceContext*.
// This class should hold a BrowserContext* when constructed on the UI thread
// and a ResourceContext* when constructed on the IO thread. This object must
// only be accessed on the thread it was constructed and does not allow
// converting between the two pointer types.
class BrowserOrResourceContext final {
 public:
  BrowserOrResourceContext() {
    union_.browser_context_ = nullptr;
    flavour_ = kNullFlavour;
  }

  // BrowserOrResourceContext is implicitly constructible from either
  // BrowserContext* or ResourceContext*.  Neither of the constructor arguments
  // can be null (enforced by DCHECKs and in some cases at compile time).
  explicit BrowserOrResourceContext(BrowserContext* browser_context) {
    DCHECK(browser_context);
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    union_.browser_context_ = browser_context;
    flavour_ = kBrowserContextFlavour;
  }

  explicit BrowserOrResourceContext(ResourceContext* resource_context) {
    DCHECK(resource_context);
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    union_.resource_context_ = resource_context;
    flavour_ = kResourceContextFlavour;
  }
  BrowserOrResourceContext(std::nullptr_t) = delete;

  // BrowserOrResourceContext has a trivial, default destructor.
  ~BrowserOrResourceContext() = default;

  // BrowserOrResourceContext is trivially copyable.
  BrowserOrResourceContext(const BrowserOrResourceContext& other) = default;
  BrowserOrResourceContext& operator=(const BrowserOrResourceContext& other) =
      default;

  explicit operator bool() const {
    return (union_.resource_context_ != nullptr &&
            union_.browser_context_ != nullptr);
  }

  // To be called only on the UI thread.  In DCHECK-enabled builds will verify
  // that this object has kBrowserContextFlavour (implying that the returned
  // BrowserContext* is valid and non-null.
  BrowserContext* ToBrowserContext() const {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    CHECK_EQ(kBrowserContextFlavour, flavour_);
    return union_.browser_context_;
  }

  // To be called only on the IO thread.  In DCHECK-enabled builds will verify
  // that this object has kResourceContextFlavour (implying that the returned
  // ResourceContext* is valid and non-null.
  ResourceContext* ToResourceContext() const {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    CHECK_EQ(kResourceContextFlavour, flavour_);
    return union_.resource_context_;
  }

 private:
  union Union {
    BrowserContext* browser_context_;
    ResourceContext* resource_context_;
  } union_;

  enum Flavour {
    kNullFlavour,
    kBrowserContextFlavour,
    kResourceContextFlavour,
  } flavour_;
};

static_assert(
    std::is_trivially_copyable<BrowserOrResourceContext>::value,
    "BrowserOrResourceContext should be trivially copyable in release builds.");

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_OR_RESOURCE_CONTEXT_H_
