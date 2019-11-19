// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_BACKEND_IMPL_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_BACKEND_IMPL_H_

#include <stdint.h>
#include <memory>

#include "content/browser/appcache/appcache_host.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"

namespace content {

class AppCacheServiceImpl;

class CONTENT_EXPORT AppCacheBackendImpl
    : public blink::mojom::AppCacheBackend {
 public:
  AppCacheBackendImpl(AppCacheServiceImpl* service,
                      int process_id,
                      int routing_id);
  ~AppCacheBackendImpl() override;

  // blink::mojom::AppCacheBackend
  void RegisterHost(
      mojo::PendingReceiver<blink::mojom::AppCacheHost> host_receiver,
      mojo::PendingRemote<blink::mojom::AppCacheFrontend> frontend_remote,
      const base::UnguessableToken& host_id) override;

 private:
  // Raw pointer is safe because instances of this class are owned by
  // |service_|.
  AppCacheServiceImpl* service_;
  const int process_id_;
  const int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheBackendImpl);
};

}  // namespace

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_BACKEND_IMPL_H_
