// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_WEB_URL_LOADER_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_WEB_URL_LOADER_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "third_party/blink/public/mojom/loader/keep_alive_handle_factory.mojom-blink.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace blink {

class BackForwardCacheLoaderHelper;
class WebURLLoader;
class WebURLRequest;

// An abstract interface to create a URLLoader. It is expected that each
// loading context holds its own per-context WebURLLoaderFactory.
class BLINK_PLATFORM_EXPORT WebURLLoaderFactory {
 public:
  WebURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      const Vector<String>& cors_exempt_header_list,
      base::WaitableEvent* terminate_sync_load_event);
  WebURLLoaderFactory();
  WebURLLoaderFactory(const WebURLLoaderFactory&) = delete;
  WebURLLoaderFactory& operator=(const WebURLLoaderFactory&) = delete;
  virtual ~WebURLLoaderFactory();

  // Returns a new WebURLLoader instance. This should internally choose
  // the most appropriate URLLoaderFactory implementation.
  // TODO(yuzus): Only take unfreezable task runner once both
  // URLLoaderClientImpl and ResponseBodyLoader use unfreezable task runner.
  // This currently takes two task runners: freezable and unfreezable one.
  virtual std::unique_ptr<WebURLLoader> CreateURLLoader(
      const WebURLRequest& webreq,
      scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
      mojo::PendingRemote<mojom::blink::KeepAliveHandle> keep_alive_handle,
      BackForwardCacheLoaderHelper* back_forward_cache_loader_helper);

 protected:
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;
  Vector<String> cors_exempt_header_list_;
  base::WaitableEvent* terminate_sync_load_event_ = nullptr;
};

// A test version of the above factory interface, which supports cloning the
// factory.
class WebURLLoaderFactoryForTest : public WebURLLoaderFactory {
 public:
  // Clones this factory.
  virtual std::unique_ptr<WebURLLoaderFactoryForTest> Clone() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_WEB_URL_LOADER_FACTORY_H_
