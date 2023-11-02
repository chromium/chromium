// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_SHARED_CORS_ORIGIN_ACCESS_LIST_IMPL_H_
#define CONTENT_BROWSER_LOADER_SHARED_CORS_ORIGIN_ACCESS_LIST_IMPL_H_

#include "content/public/browser/shared_cors_origin_access_list.h"
#include "services/network/public/cpp/cors/origin_access_list.h"

namespace content {

// SharedCorsOriginAccessList implementation class.
class SharedCorsOriginAccessListImpl final : public SharedCorsOriginAccessList {
 public:
  SharedCorsOriginAccessListImpl();

  SharedCorsOriginAccessListImpl(const SharedCorsOriginAccessListImpl&) =
      delete;
  SharedCorsOriginAccessListImpl& operator=(
      const SharedCorsOriginAccessListImpl&) = delete;

  // SharedCorsOriginAccessList interface.
  void SetForOrigin(
      const url::Origin& source_origin,
      std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
      std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
      base::OnceClosure closure) override;
  const network::cors::OriginAccessList& GetOriginAccessList() override;

 protected:
  ~SharedCorsOriginAccessListImpl() override;

 private:
  network::cors::OriginAccessList origin_access_list_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_SHARED_CORS_ORIGIN_ACCESS_LIST_IMPL_H_
