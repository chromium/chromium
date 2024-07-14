// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/p2p/mdns_responder_adapter.h"

#include <string>

#include "components/webrtc/net_address_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/mojom/mdns_responder.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/rtc_base/ip_address.h"

namespace blink {

namespace {

void OnNameCreatedForAddress(
    webrtc::MdnsResponderInterface::NameCreatedCallback callback,
    const rtc::IPAddress& addr,
    const String& name,
    bool announcement_scheduled) {
  // We currently ignore whether there is an announcement sent for the name.
  callback(addr, name.Utf8());
}

void OnNameRemovedForAddress(
    webrtc::MdnsResponderInterface::NameRemovedCallback callback,
    bool removed,
    bool goodbye_scheduled) {
  // We currently ignore whether there is a goodbye sent for the name.
  callback(removed);
}

}  // namespace

MdnsResponderAdapter::MdnsResponderAdapter(MojoBindingContext& context) {
  mojo::PendingRemote<network::mojom::blink::MdnsResponder> client;
  auto receiver = client.InitWithNewPipeAndPassReceiver();
  shared_remote_client_ =
      mojo::SharedRemote<network::mojom::blink::MdnsResponder>(
          std::move(client));
  context.GetBrowserInterfaceBroker().GetInterface(std::move(receiver));
}

MdnsResponderAdapter::~MdnsResponderAdapter() = default;

void MdnsResponderAdapter::CreateNameForAddress(const rtc::IPAddress& addr,
                                                NameCreatedCallback callback) {
  shared_remote_client_->CreateNameForAddress(
      webrtc::RtcIPAddressToNetIPAddress(addr),
      WTF::BindOnce(&OnNameCreatedForAddress, callback, addr));
}

void MdnsResponderAdapter::RemoveNameForAddress(const rtc::IPAddress& addr,
                                                NameRemovedCallback callback) {
  shared_remote_client_->RemoveNameForAddress(
      webrtc::RtcIPAddressToNetIPAddress(addr),
      WTF::BindOnce(&OnNameRemovedForAddress, callback));
}

}  // namespace blink
