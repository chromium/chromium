// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AD_AUCTION_NAVIGATOR_AUCTION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AD_AUCTION_NAVIGATOR_AUCTION_H_

#include <stdint.h>

#include "third_party/blink/public/mojom/interest_group/ad_auction_service.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class AdRequestConfig;
class Ads;
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
  // implicit leaveAdInterestGroup - only supported when called from within
  // a fenced frame showing FLEDGE ads.
  void leaveAdInterestGroupForDocument(ScriptState*, ExceptionState&);
  static void leaveAdInterestGroup(ScriptState*, Navigator&, ExceptionState&);

  void updateAdInterestGroups();
  static void updateAdInterestGroups(ScriptState*, Navigator&, ExceptionState&);
  ScriptPromise runAdAuction(ScriptState*,
                             const AuctionAdConfig*,
                             ExceptionState&);
  static ScriptPromise runAdAuction(ScriptState*,
                                    Navigator&,
                                    const AuctionAdConfig*,
                                    ExceptionState&);

  // If called from a FencedFrame that was navigated to the URN resulting from
  // an interest group ad auction, returns a Vector of ad component URNs
  // associated with the winning bid in that auction.
  //
  // `num_ad_components` is the number of ad component URNs to put in the
  // Vector. To avoid leaking data from the winning bidder worklet, the number
  // of ad components in the winning bid is not exposed. Instead, it's padded
  // with URNs to length kMaxAdAuctionAdComponents, and calling this method
  // returns the first `num_ad_components` URNs.
  //
  // Throws an exception if `num_ad_components` is greater than
  // kMaxAdAuctionAdComponents, or if called from a frame that was not navigated
  // to a URN representing the winner of an ad auction.
  static Vector<String> adAuctionComponents(ScriptState* script_state,
                                            Navigator& navigator,
                                            uint16_t num_ad_components,
                                            ExceptionState& exception_state);

  ScriptPromise deprecatedURNToURL(ScriptState* script_state,
                                   const String& uuid_url_string,
                                   ExceptionState& exception_state);

  static ScriptPromise deprecatedURNToURL(ScriptState* script_state,
                                          Navigator& navigator,
                                          const String& uuid_url_string,
                                          ExceptionState& exception_state);

  ScriptPromise createAdRequest(ScriptState*,
                                const AdRequestConfig*,
                                ExceptionState&);
  static ScriptPromise createAdRequest(ScriptState*,
                                       Navigator&,
                                       const AdRequestConfig*,
                                       ExceptionState&);
  ScriptPromise finalizeAd(ScriptState*,
                           const Ads*,
                           const AuctionAdConfig*,
                           ExceptionState&);
  static ScriptPromise finalizeAd(ScriptState*,
                                  Navigator&,
                                  const Ads*,
                                  const AuctionAdConfig*,
                                  ExceptionState&);

  void Trace(Visitor* visitor) const override {
    visitor->Trace(ad_auction_service_);
    Supplement<Navigator>::Trace(visitor);
  }

 private:
  // Completion callback for createAdRequest() mojo call.
  void AdsRequested(ScriptPromiseResolver* resolver,
                    const WTF::String& ads_guid);
  // Completion callback for finalizeAd() mojo call.
  void FinalizeAdComplete(ScriptPromiseResolver* resolver,
                          const absl::optional<KURL>& creative_url);
  // Completion callback for Mojo call made by runAdAuction().
  void AuctionComplete(ScriptPromiseResolver*, const absl::optional<KURL>&);
  // Completion callback for Mojo call made by deprecatedURNToURL().
  void GetURLFromURNComplete(ScriptPromiseResolver*,
                             const absl::optional<KURL>&);

  HeapMojoRemote<mojom::blink::AdAuctionService> ad_auction_service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AD_AUCTION_NAVIGATOR_AUCTION_H_
