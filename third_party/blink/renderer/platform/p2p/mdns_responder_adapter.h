// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_P2P_MDNS_RESPONDER_ADAPTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_P2P_MDNS_RESPONDER_ADAPTER_H_

#include "mojo/public/cpp/bindings/shared_remote.h"
#include "services/network/public/mojom/mdns_responder.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/webrtc/rtc_base/mdns_responder_interface.h"

namespace rtc {
class IPAddress;
}  // namespace rtc

namespace blink {

// This class is created on the main thread but is used only on the WebRTC
// worker threads. The MdnsResponderAdapter implements the WebRTC mDNS responder
// interface via the MdnsResponder service in Chromium, and is used to register
// and resolve mDNS hostnames to conceal local IP addresses.
class PLATFORM_EXPORT MdnsResponderAdapter
    : public webrtc::MdnsResponderInterface {
 public:
  // The adapter should be created on the main thread to have access to the
  // connector to the service manager.
  MdnsResponderAdapter();
  ~MdnsResponderAdapter() override;

  // webrtc::MdnsResponderInterface implementation.
  void CreateNameForAddress(const rtc::IPAddress& addr,
                            NameCreatedCallback callback) override;
  void RemoveNameForAddress(const rtc::IPAddress& addr,
                            NameRemovedCallback callback) override;

 private:
  mojo::SharedRemote<network::mojom::blink::MdnsResponder>
      shared_remote_client_;

  DISALLOW_COPY_AND_ASSIGN(MdnsResponderAdapter);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_P2P_MDNS_RESPONDER_ADAPTER_H_
