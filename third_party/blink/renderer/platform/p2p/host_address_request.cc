// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/p2p/host_address_request.h"

#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/location.h"
#include "components/webrtc/net_address_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/p2p/socket_dispatcher.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

P2PAsyncAddressResolver::P2PAsyncAddressResolver(
    P2PSocketDispatcher* dispatcher)
    : dispatcher_(dispatcher), state_(kStateCreated) {}

P2PAsyncAddressResolver::~P2PAsyncAddressResolver() {
  DCHECK(state_ == kStateCreated || state_ == kStateFinished);
}

void P2PAsyncAddressResolver::Start(const rtc::SocketAddress& host_name,
                                    std::optional<int> address_family,
                                    DoneCallback done_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(kStateCreated, state_);
  DCHECK(dispatcher_);

  state_ = kStateSent;
  done_callback_ = std::move(done_callback);
  bool enable_mdns = base::FeatureList::IsEnabled(
      blink::features::kWebRtcHideLocalIpsWithMdns);
  auto callback = WTF::BindOnce(&P2PAsyncAddressResolver::OnResponse,
                                scoped_refptr<P2PAsyncAddressResolver>(this));
  if (address_family.has_value()) {
    dispatcher_->GetP2PSocketManager()->GetHostAddressWithFamily(
        String(host_name.hostname().data()), address_family.value(),
        enable_mdns, std::move(callback));
  } else {
    dispatcher_->GetP2PSocketManager()->GetHostAddress(
        String(host_name.hostname().data()), enable_mdns, std::move(callback));
  }
  dispatcher_ = nullptr;
}

void P2PAsyncAddressResolver::Cancel() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (state_ != kStateFinished)
    state_ = kStateFinished;

  done_callback_.Reset();
}

void P2PAsyncAddressResolver::OnResponse(
    const Vector<net::IPAddress>& addresses) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (state_ == kStateSent) {
    state_ = kStateFinished;
    std::move(done_callback_).Run(addresses);
  }
}

}  // namespace blink
