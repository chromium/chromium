// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SHARED_CORS_ORIGIN_ACCESS_LIST_H_
#define CONTENT_PUBLIC_BROWSER_SHARED_CORS_ORIGIN_ACCESS_LIST_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom.h"
#include "url/origin.h"

namespace network {
namespace cors {
class OriginAccessList;
}  // namespace cors
}  // namespace network

namespace content {

// A public interface to manage CORS origin access lists on the UI thread.
// The shared network::cors::OriginAccessList instance can only be accessed on
// the IO thread if NetworkService is not enabled. Callers on UI thread must use
// this wrapper class to make it work with and without NetworkService until
// NetworkService is fully enabled. If NetworkService is enabled,
// network::cors::OriginAccessList is accessed only on the UI thread, and all
// calls can be finished synchronously. This is used for remembering per-profile
// access lists in the browser process.
// TODO(toyoshim): Remove this class, and use network::cors::OriginAccessList
// directly once NetworkService is fully enabled.
class CONTENT_EXPORT SharedCorsOriginAccessList
    : public base::RefCountedThreadSafe<SharedCorsOriginAccessList> {
 public:
  static scoped_refptr<SharedCorsOriginAccessList> Create();

  SharedCorsOriginAccessList() = default;

  // Sets the access list to an internal network::cors::OriginAccessList
  // instance so that its IsAllowed() method works for all users that refer the
  // shared network::cors::OriginAccessList instance returned by
  // origin_access_list() below. |allow_patterns| and |block_patterns| will be
  // moved so to pass the lists to the IO thread if NetworkService is disabled.
  // Should be called on the UI thread, and |closure| runs on the UI thread too.
  virtual void SetForOrigin(
      const url::Origin& source_origin,
      std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
      std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
      base::OnceClosure closure) = 0;

  // Gets a shared OriginAccessList instance pointer. |this| should outlives
  // callers' OriginAccessList instance uses. Should be called on the IO thread.
  virtual const network::cors::OriginAccessList& GetOriginAccessList() = 0;

 protected:
  virtual ~SharedCorsOriginAccessList() = default;

 private:
  friend class base::RefCountedThreadSafe<SharedCorsOriginAccessList>;

  DISALLOW_COPY_AND_ASSIGN(SharedCorsOriginAccessList);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SHARED_CORS_ORIGIN_ACCESS_LIST_H_
