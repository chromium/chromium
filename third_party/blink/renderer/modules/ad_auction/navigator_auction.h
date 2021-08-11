// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AD_AUCTION_NAVIGATOR_AUCTION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AD_AUCTION_NAVIGATOR_AUCTION_H_

#include "third_party/blink/public/mojom/interest_group/ad_auction_service.mojom-blink.h"
#include "third_party/blink/public/mojom/interest_group/restricted_interest_group_store.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class AuctionAdInterestGroup;
class AuctionAdConfig;
class ScriptPromiseResolver;

class MODULES_EXPORT NavigatorAuction final
    : public GarbageCollected<NavigatorAuction>,
      public Supplement<Navigator> {
 public:
  static const char kSupplementName[];

  explicit NavigatorAuction(Navigator&);

  // Gets, or creates, NavigatorAuction supplement on Navigator.
  // See platform/Supplementable.h
  static NavigatorAuction& From(ExecutionContext*, Navigator&);

  void joinAdInterestGroup(ScriptState*,
                           const AuctionAdInterestGroup*,
                           double,
                           ExceptionState&);
  static void joinAdInterestGroup(ScriptState*,
                                  Navigator&,
                                  const AuctionAdInterestGroup*,
                                  double,
                                  ExceptionState&);
  void leaveAdInterestGroup(ScriptState*,
                            const AuctionAdInterestGroup*,
                            ExceptionState&);
  static void leaveAdInterestGroup(ScriptState*,
                                   Navigator&,
                                   const AuctionAdInterestGroup*,
                                   ExceptionState&);
  void updateAdInterestGroups();
  static void updateAdInterestGroups(ScriptState*, Navigator&);
  ScriptPromise runAdAuction(ScriptState*,
                             const AuctionAdConfig*,
                             ExceptionState&);
  static ScriptPromise runAdAuction(ScriptState*,
                                    Navigator&,
                                    const AuctionAdConfig*,
                                    ExceptionState&);

  void Trace(Visitor* visitor) const override {
    visitor->Trace(interest_group_store_);
    visitor->Trace(ad_auction_service_);
    Supplement<Navigator>::Trace(visitor);
  }

 private:
  // Completion callback for Mojo call made by runAdAuction().
  void AuctionComplete(ScriptPromiseResolver*, const absl::optional<KURL>&);

  HeapMojoRemote<mojom::blink::AdAuctionService> ad_auction_service_;
  HeapMojoRemote<mojom::blink::RestrictedInterestGroupStore>
      interest_group_store_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AD_AUCTION_NAVIGATOR_AUCTION_H_
