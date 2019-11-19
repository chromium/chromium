/*
 * Copyright (c) 2009, Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom-blink.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CORE_EXPORT ApplicationCacheHost
    : public GarbageCollected<ApplicationCacheHost>,
      public mojom::blink::AppCacheFrontend {
 public:
  ApplicationCacheHost(
      const BrowserInterfaceBrokerProxy& interface_broker_proxy,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~ApplicationCacheHost() override;
  virtual void Detach();

  struct CacheInfo {
    STACK_ALLOCATED();

   public:
    CacheInfo(const KURL& manifest,
              double creation_time,
              double update_time,
              int64_t response_sizes,
              int64_t padding_sizes)
        : manifest_(manifest),
          creation_time_(creation_time),
          update_time_(update_time),
          response_sizes_(response_sizes),
          padding_sizes_(padding_sizes) {}
    CacheInfo() = default;
    KURL manifest_;
    double creation_time_ = 0;
    double update_time_ = 0;
    int64_t response_sizes_ = 0;
    int64_t padding_sizes_ = 0;
  };

  mojom::blink::AppCacheStatus GetStatus() const;
  void Abort();

  void FillResourceList(Vector<mojom::blink::AppCacheResourceInfo>*);
  CacheInfo ApplicationCacheInfo();

  const base::UnguessableToken& GetHostID() const;
  void SetHostID(const base::UnguessableToken& host_id);

  void SelectCacheForWorker(int64_t app_cache_id,
                            base::OnceClosure completion_callback);

  // mojom::blink::AppCacheFrontend
  void CacheSelected(mojom::blink::AppCacheInfoPtr info) override;
  void EventRaised(mojom::blink::AppCacheEventID event_id) override;
  void ProgressEventRaised(const KURL& url,
                           int32_t num_total,
                           int32_t num_complete) override;
  void ErrorEventRaised(mojom::blink::AppCacheErrorDetailsPtr details) override;
  void LogMessage(mojom::blink::ConsoleMessageLevel log_level,
                  const String& message) override {}
  void SetSubresourceFactory(
      mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>
          url_loader_factory) override {}

  virtual void Trace(blink::Visitor*) {}

 protected:
  mojo::Remote<mojom::blink::AppCacheHost> backend_host_;
  mojom::blink::AppCacheStatus status_ =
      mojom::blink::AppCacheStatus::APPCACHE_STATUS_UNCACHED;

  // Non-empty |host_id_| must be set before calling this function.
  bool BindBackend();

 private:
  virtual void NotifyApplicationCache(mojom::AppCacheEventID,
                                      int progress_total,
                                      int progress_done,
                                      mojom::AppCacheErrorReason,
                                      const String& error_url,
                                      int error_status,
                                      const String& error_message) {}

  void GetAssociatedCacheInfo(CacheInfo* info);

  mojo::Receiver<mojom::blink::AppCacheFrontend> receiver_{this};
  mojo::Remote<mojom::blink::AppCacheBackend> backend_remote_;

  base::UnguessableToken host_id_;
  mojom::blink::AppCacheInfo cache_info_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  const BrowserInterfaceBrokerProxy& interface_broker_proxy_;

  // Invoked when CacheSelected() is called.
  base::OnceClosure select_cache_for_worker_completion_callback_;

  FRIEND_TEST_ALL_PREFIXES(DocumentTest, SandboxDisablesAppCache);

  DISALLOW_COPY_AND_ASSIGN(ApplicationCacheHost);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_H_
