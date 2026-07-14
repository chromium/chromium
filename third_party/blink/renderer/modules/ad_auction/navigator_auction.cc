// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ad_auction/navigator_auction.h"

#include <optional>
#include <utility>

#include "base/uuid.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ad_auction/protected_audience.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

template <typename T>
ScriptPromise<T> ResolveUndefined(ScriptState* script_state) {
  return ScriptPromise<T>::FromV8Value(
      script_state, v8::Undefined(script_state->GetIsolate()));
}

template <typename T>
ScriptPromise<T> ResolveNull(ScriptState* script_state) {
  return ScriptPromise<T>::FromV8Value(script_state,
                                       v8::Null(script_state->GetIsolate()));
}

}  // namespace

const char NavigatorAuction::kSupplementName[] = "NavigatorAuction";

NavigatorAuction::NavigatorAuction(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

NavigatorAuction& NavigatorAuction::From(ExecutionContext* context,
                                         Navigator& navigator) {
  NavigatorAuction* supplement =
      Supplement<Navigator>::From<NavigatorAuction>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorAuction>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

ScriptPromise<IDLUndefined> NavigatorAuction::joinAdInterestGroup(
    ScriptState* script_state,
    AuctionAdInterestGroup* group,
    std::optional<double> duration,
    ExceptionState& exception_state) {
  return ResolveUndefined<IDLUndefined>(script_state);
}

ScriptPromise<IDLUndefined> NavigatorAuction::joinAdInterestGroup(
    ScriptState* script_state,
    Navigator& navigator,
    AuctionAdInterestGroup* group,
    double duration,
    ExceptionState& exception_state) {
  return From(ExecutionContext::From(script_state), navigator)
      .joinAdInterestGroup(script_state, group, duration, exception_state);
}

ScriptPromise<IDLUndefined> NavigatorAuction::joinAdInterestGroup(
    ScriptState* script_state,
    Navigator& navigator,
    AuctionAdInterestGroup* group,
    ExceptionState& exception_state) {
  return From(ExecutionContext::From(script_state), navigator)
      .joinAdInterestGroup(script_state, group, std::nullopt, exception_state);
}

ScriptPromise<IDLUndefined> NavigatorAuction::leaveAdInterestGroup(
    ScriptState* script_state,
    const AuctionAdInterestGroupKey* group_key,
    ExceptionState& exception_state) {
  return ResolveUndefined<IDLUndefined>(script_state);
}

ScriptPromise<IDLUndefined> NavigatorAuction::leaveAdInterestGroup(
    ScriptState* script_state,
    Navigator& navigator,
    const AuctionAdInterestGroupKey* group_key,
    ExceptionState& exception_state) {
  return From(ExecutionContext::From(script_state), navigator)
      .leaveAdInterestGroup(script_state, group_key, exception_state);
}

ScriptPromise<IDLUndefined> NavigatorAuction::leaveAdInterestGroupForDocument(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return ResolveUndefined<IDLUndefined>(script_state);
}

ScriptPromise<IDLUndefined> NavigatorAuction::leaveAdInterestGroup(
    ScriptState* script_state,
    Navigator& navigator,
    ExceptionState& exception_state) {
  return From(ExecutionContext::From(script_state), navigator)
      .leaveAdInterestGroupForDocument(script_state, exception_state);
}

ScriptPromise<IDLUndefined> NavigatorAuction::clearOriginJoinedAdInterestGroups(
    ScriptState* script_state,
    const String& owner,
    Vector<String> interest_groups_to_keep,
    ExceptionState& exception_state) {
  return ResolveUndefined<IDLUndefined>(script_state);
}

ScriptPromise<IDLUndefined> NavigatorAuction::clearOriginJoinedAdInterestGroups(
    ScriptState* script_state,
    Navigator& navigator,
    const String owner,
    ExceptionState& exception_state) {
  return From(ExecutionContext::From(script_state), navigator)
      .clearOriginJoinedAdInterestGroups(script_state, owner, Vector<String>(),
                                         exception_state);
}

ScriptPromise<IDLUndefined> NavigatorAuction::clearOriginJoinedAdInterestGroups(
    ScriptState* script_state,
    Navigator& navigator,
    const String owner,
    const Vector<String> interest_groups_to_keep,
    ExceptionState& exception_state) {
  return From(ExecutionContext::From(script_state), navigator)
      .clearOriginJoinedAdInterestGroups(
          script_state, owner, interest_groups_to_keep, exception_state);
}

void NavigatorAuction::updateAdInterestGroups() {}

void NavigatorAuction::updateAdInterestGroups(ScriptState* script_state,
                                              Navigator& navigator,
                                              ExceptionState& exception_state) {
}

ScriptPromise<IDLNullable<V8UnionFencedFrameConfigOrUSVString>>
NavigatorAuction::runAdAuction(ScriptState* script_state,
                               AuctionAdConfig* config,
                               ExceptionState& exception_state,
                               base::TimeTicks start_time) {
  return ResolveNull<IDLNullable<V8UnionFencedFrameConfigOrUSVString>>(
      script_state);
}

ScriptPromise<IDLNullable<V8UnionFencedFrameConfigOrUSVString>>
NavigatorAuction::runAdAuction(ScriptState* script_state,
                               Navigator& navigator,
                               AuctionAdConfig* config,
                               ExceptionState& exception_state) {
  return From(ExecutionContext::From(script_state), navigator)
      .runAdAuction(script_state, config, exception_state);
}

ScriptPromise<IDLString> NavigatorAuction::createAuctionNonce(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return ScriptPromise<IDLString>::FromV8Value(
      script_state,
      V8String(script_state->GetIsolate(),
               base::Uuid::GenerateRandomV4().AsLowercaseString().c_str()));
}

ScriptPromise<IDLString> NavigatorAuction::createAuctionNonce(
    ScriptState* script_state,
    Navigator& navigator,
    ExceptionState& exception_state) {
  return From(ExecutionContext::From(script_state), navigator)
      .createAuctionNonce(script_state, exception_state);
}

Vector<String> NavigatorAuction::adAuctionComponents(
    ScriptState* script_state,
    Navigator& navigator,
    uint16_t num_ad_components,
    ExceptionState& exception_state) {
  return Vector<String>();
}

ScriptPromise<IDLUSVString> NavigatorAuction::deprecatedURNToURL(
    ScriptState* script_state,
    const String& urn_uuid,
    bool send_reports,
    ExceptionState& exception_state) {
  return ScriptPromise<IDLUSVString>::FromV8Value(
      script_state, V8String(script_state->GetIsolate(), "about:blank"));
}

ScriptPromise<IDLUSVString> NavigatorAuction::deprecatedURNToURL(
    ScriptState* script_state,
    Navigator& navigator,
    const V8UnionFencedFrameConfigOrUSVString* urn_or_config,
    bool send_reports,
    ExceptionState& exception_state) {
  return ScriptPromise<IDLUSVString>::FromV8Value(
      script_state, V8String(script_state->GetIsolate(), "about:blank"));
}

ScriptPromise<IDLUndefined> NavigatorAuction::deprecatedReplaceInURN(
    ScriptState* script_state,
    const String& urn_uuid,
    const Vector<std::pair<String, String>>& replacement,
    ExceptionState& exception_state) {
  return ResolveUndefined<IDLUndefined>(script_state);
}

ScriptPromise<IDLUndefined> NavigatorAuction::deprecatedReplaceInURN(
    ScriptState* script_state,
    Navigator& navigator,
    const V8UnionFencedFrameConfigOrUSVString* urn_or_config,
    const Vector<std::pair<String, String>>& replacement,
    ExceptionState& exception_state) {
  return ResolveUndefined<IDLUndefined>(script_state);
}

ScriptPromise<AdAuctionData> NavigatorAuction::getInterestGroupAdAuctionData(
    ScriptState* script_state,
    const AdAuctionDataConfig* config,
    ExceptionState& exception_state,
    base::TimeTicks start_time) {
  return ResolveUndefined<AdAuctionData>(script_state);
}

ScriptPromise<AdAuctionData> NavigatorAuction::getInterestGroupAdAuctionData(
    ScriptState* script_state,
    Navigator& navigator,
    const AdAuctionDataConfig* config,
    ExceptionState& exception_state) {
  return ResolveUndefined<AdAuctionData>(script_state);
}

ScriptPromise<Ads> NavigatorAuction::createAdRequest(
    ScriptState* script_state,
    const AdRequestConfig* config,
    ExceptionState& exception_state) {
  return ResolveUndefined<Ads>(script_state);
}

ScriptPromise<Ads> NavigatorAuction::createAdRequest(
    ScriptState* script_state,
    Navigator& navigator,
    const AdRequestConfig* config,
    ExceptionState& exception_state) {
  return ResolveUndefined<Ads>(script_state);
}

ScriptPromise<IDLString> NavigatorAuction::finalizeAd(
    ScriptState* script_state,
    const Ads* ads,
    const AuctionAdConfig* config,
    ExceptionState& exception_state) {
  return ScriptPromise<IDLString>::FromV8Value(
      script_state, V8String(script_state->GetIsolate(), ""));
}

ScriptPromise<IDLString> NavigatorAuction::finalizeAd(
    ScriptState* script_state,
    Navigator& navigator,
    const Ads* ads,
    const AuctionAdConfig* config,
    ExceptionState& exception_state) {
  return ScriptPromise<IDLString>::FromV8Value(
      script_state, V8String(script_state->GetIsolate(), ""));
}

bool NavigatorAuction::canLoadAdAuctionFencedFrame(ScriptState*) {
  return false;
}

bool NavigatorAuction::canLoadAdAuctionFencedFrame(ScriptState*, Navigator&) {
  return false;
}

bool NavigatorAuction::deprecatedRunAdAuctionEnforcesKAnonymity(ScriptState*,
                                                                Navigator&) {
  return false;
}

ProtectedAudience* NavigatorAuction::protectedAudience(
    ScriptState* script_state,
    Navigator& navigator) {
  if (!navigator.DomWindow()) {
    return nullptr;
  }
  NavigatorAuction& supplement =
      From(ExecutionContext::From(script_state), navigator);
  if (!supplement.protected_audience_) {
    supplement.protected_audience_ = MakeGarbageCollected<ProtectedAudience>(
        ExecutionContext::From(script_state));
  }
  return supplement.protected_audience_.Get();
}

}  // namespace blink
