// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/p2p/host_address_request.h"

#include <utility>

#include "base/feature_list.h"
#include "base/location.h"
#include "jingle/glue/utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/p2p/socket_dispatcher.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

P2PAsyncAddressResolver::P2PAsyncAddressResolver(
    P2PSocketDispatcher* dispatcher)
    : dispatcher_(dispatcher), state_(STATE_CREATED) {
}

P2PAsyncAddressResolver::~P2PAsyncAddressResolver() {
  DCHECK(state_ == STATE_CREATED || state_ == STATE_FINISHED);
}

void P2PAsyncAddressResolver::Start(const rtc::SocketAddress& host_name,
                                    DoneCallback done_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(STATE_CREATED, state_);

  state_ = STATE_SENT;
  done_callback_ = std::move(done_callback);
  bool enable_mdns = base::FeatureList::IsEnabled(
      blink::features::kWebRtcHideLocalIpsWithMdns);
  dispatcher_->GetP2PSocketManager()->GetHostAddress(
      String(host_name.hostname().data()), enable_mdns,
      WTF::Bind(&P2PAsyncAddressResolver::OnResponse,
                scoped_refptr<P2PAsyncAddressResolver>(this)));
}

void P2PAsyncAddressResolver::Cancel() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (state_ != STATE_FINISHED)
    state_ = STATE_FINISHED;

  done_callback_.Reset();
}

void P2PAsyncAddressResolver::OnResponse(
    const Vector<net::IPAddress>& addresses) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (state_ == STATE_SENT) {
    state_ = STATE_FINISHED;
    std::move(done_callback_).Run(addresses);
  }
}

}  // namespace blink
