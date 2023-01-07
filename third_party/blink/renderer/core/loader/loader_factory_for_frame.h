// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_LOADER_FACTORY_FOR_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_LOADER_FACTORY_FOR_FRAME_H_

#include <memory>
#include <utility>
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

namespace blink {

class DocumentLoader;
class LocalDOMWindow;
class PrefetchedSignedExchangeManager;

class LoaderFactoryForFrame final : public ResourceFetcher::LoaderFactory {
 public:
  LoaderFactoryForFrame(DocumentLoader& loader, LocalDOMWindow& window);

  void Trace(Visitor*) const override;

  // LoaderFactory implementations
  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const ResourceRequest&,
      const ResourceLoaderOptions&,
      scoped_refptr<base::SingleThreadTaskRunner>,
      scoped_refptr<base::SingleThreadTaskRunner>,
      WebBackForwardCacheLoaderHelper) override;
  std::unique_ptr<WebCodeCacheLoader> CreateCodeCacheLoader() override;

  std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
  CreateTaskRunnerHandle(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

 private:
  void IssueKeepAliveHandleIfRequested(
      const ResourceRequest& request,
      mojom::blink::LocalFrameHost& local_frame_host,
      mojo::PendingReceiver<mojom::blink::KeepAliveHandle> pending_receiver);

  const Member<DocumentLoader> document_loader_;
  const Member<LocalDOMWindow> window_;
  const Member<PrefetchedSignedExchangeManager>
      prefetched_signed_exchange_manager_;
  HeapMojoRemote<blink::mojom::blink::KeepAliveHandleFactory>
      keep_alive_handle_factory_;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_LOADER_FACTORY_FOR_FRAME_H_
