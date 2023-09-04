// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_LOADER_FACTORY_FOR_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_LOADER_FACTORY_FOR_FRAME_H_

#include <memory>
#include <utility>
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/keep_alive_handle.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/keep_alive_handle_factory.mojom-blink.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class DocumentLoader;
class LocalDOMWindow;
class PrefetchedSignedExchangeManager;

class CORE_EXPORT LoaderFactoryForFrame final
    : public ResourceFetcher::LoaderFactory {
 public:
  static void SetCorsExemptHeaderList(Vector<String>);
  static Vector<String> GetCorsExemptHeaderList();

  LoaderFactoryForFrame(DocumentLoader& loader, LocalDOMWindow& window);

  void Trace(Visitor*) const override;

  // LoaderFactory implementations
  std::unique_ptr<URLLoader> CreateURLLoader(
      const ResourceRequest&,
      const ResourceLoaderOptions&,
      scoped_refptr<base::SingleThreadTaskRunner>,
      scoped_refptr<base::SingleThreadTaskRunner>,
      BackForwardCacheLoaderHelper*) override;
  std::unique_ptr<WebCodeCacheLoader> CreateCodeCacheLoader() override;

 private:
  void IssueKeepAliveHandleIfRequested(
      const ResourceRequest& request,
      mojom::blink::LocalFrameHost& local_frame_host,
      mojo::PendingReceiver<mojom::blink::KeepAliveHandle> pending_receiver);

  const Member<DocumentLoader> document_loader_;
  const Member<LocalDOMWindow> window_;
  const Member<PrefetchedSignedExchangeManager>
      prefetched_signed_exchange_manager_;
  std::unique_ptr<WebURLLoaderThrottleProviderForFrame> throttle_provider_;
  HeapMojoRemote<mojom::blink::KeepAliveHandleFactory>
      keep_alive_handle_factory_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_LOADER_FACTORY_FOR_FRAME_H_
