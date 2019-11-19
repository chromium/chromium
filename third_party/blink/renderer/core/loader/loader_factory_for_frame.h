// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_LOADER_FACTORY_FOR_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_LOADER_FACTORY_FOR_FRAME_H_

#include <memory>
#include <utility>
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

namespace blink {

class FrameOrImportedDocument;
class PrefetchedSignedExchangeManager;

class LoaderFactoryForFrame final : public ResourceFetcher::LoaderFactory {
 public:
  explicit LoaderFactoryForFrame(const FrameOrImportedDocument&);

  void Trace(Visitor*) override;

  // LoaderFactory implementations
  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const ResourceRequest&,
      const ResourceLoaderOptions&,
      scoped_refptr<base::SingleThreadTaskRunner>) override;
  std::unique_ptr<CodeCacheLoader> CreateCodeCacheLoader() override;

 private:
  const Member<const FrameOrImportedDocument> frame_or_imported_document_;
  const Member<PrefetchedSignedExchangeManager>
      prefetched_signed_exchange_manager_;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_LOADER_FACTORY_FOR_FRAME_H_
