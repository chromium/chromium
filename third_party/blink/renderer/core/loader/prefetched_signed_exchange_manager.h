// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PREFETCHED_SIGNED_EXCHANGE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PREFETCHED_SIGNED_EXCHANGE_MANAGER_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-blink-forward.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/renderer/core/loader/preload_helper.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class AlternateSignedExchangeResourceInfo;
class LocalFrame;
class WebURLLoader;
class WebURLRequest;

// For SignedExchangeSubresourcePrefetch feature. This class holds the
// prefetched signed exchange info and will returns loaders for matching
// requests.
class PrefetchedSignedExchangeManager final
    : public GarbageCollected<PrefetchedSignedExchangeManager> {
 public:
  // If threre are no "allowed-alt-sxg" link headers in |inner_link_header|,
  // or |prefetched_signed_exchanges| is empty, returns null.
  static PrefetchedSignedExchangeManager* MaybeCreate(
      LocalFrame* frame,
      const String& outer_link_header,
      const String& inner_link_header,
      WebVector<std::unique_ptr<WebNavigationParams::PrefetchedSignedExchange>>
          prefetched_signed_exchanges);

  PrefetchedSignedExchangeManager(
      LocalFrame* frame,
      std::unique_ptr<AlternateSignedExchangeResourceInfo>
          alternative_resources,
      HashMap<KURL,
              std::unique_ptr<WebNavigationParams::PrefetchedSignedExchange>>
          prefetched_exchanges_map);
  ~PrefetchedSignedExchangeManager();

  void Trace(blink::Visitor* visitor);

  // Returns a loader if there is a matching resource in
  // |alternative_resources_|, otherwise returns null. This only checks the
  // existence of matching "allowed-alt-sxg" link header in the inner response.
  // This doesn't check the existence of matching "alternate" link header in the
  // outer response nor the existence of the matching prefetched signed exchange
  // in |prefetched_signed_exchanges|. This check is done in
  // StartPrefetchedLinkHeaderPreloads().
  //
  // The returned loader doesn't start loading until
  // StartPrefetchedLinkHeaderPreloads() will be called.
  std::unique_ptr<WebURLLoader> MaybeCreateURLLoader(
      const WebURLRequest& request);

  // If the all loaders which have been created by MaybeCreateURLLoader() have
  // a matching "alternate" link header in the outer response and the matching
  // prefetched signed exchange in |prefetched_exchanges_map_|, they will start
  // loading the prefetched signed exchange using the loader_factory_handle in
  // PrefetchedSignedExchange. Otherwise they will start loading using the
  // default loader. This check is intended to prevent the signed exchange
  // distributor from sending arbitrary information to the publisher of the
  // signed exchange by choosing arbitrary subresources to be loaded from
  // signed exchange.
  //
  // This method must be called when all link header preload requests have been
  // dispatched. After this method is called, MaybeCreateURLLoader() will always
  // return null.
  void StartPrefetchedLinkHeaderPreloads();

 private:
  class PrefetchedSignedExchangeLoader;

  void TriggerLoad();
  std::unique_ptr<WebURLLoader> CreateDefaultURLLoader(
      const WebURLRequest& request);
  std::unique_ptr<WebURLLoader> CreatePrefetchedSignedExchangeURLLoader(
      const WebURLRequest& request,
      mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>
          loader_factory);

  Member<LocalFrame> frame_;
  std::unique_ptr<AlternateSignedExchangeResourceInfo> alternative_resources_;
  HashMap<KURL, std::unique_ptr<WebNavigationParams::PrefetchedSignedExchange>>
      prefetched_exchanges_map_;
  bool started_ = false;

  WTF::Vector<base::WeakPtr<PrefetchedSignedExchangeLoader>> loaders_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchedSignedExchangeManager);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PREFETCHED_SIGNED_EXCHANGE_MANAGER_H_
