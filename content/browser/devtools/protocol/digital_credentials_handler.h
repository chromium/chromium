// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DIGITAL_CREDENTIALS_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DIGITAL_CREDENTIALS_HANDLER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/digital_credentials.h"
#include "content/common/content_export.h"

namespace content {

class RenderFrameHostImpl;

namespace protocol {

// DevTools handler for the "DigitalCredentials" domain. Exposes commands to
// configure a virtual wallet for automated testing of the Digital Credentials
// API.
class DigitalCredentialsHandler : public DevToolsDomainHandler,
                                  public DigitalCredentials::Backend {
 public:
  CONTENT_EXPORT DigitalCredentialsHandler();
  CONTENT_EXPORT ~DigitalCredentialsHandler() override;

  DigitalCredentialsHandler(const DigitalCredentialsHandler&) = delete;
  DigitalCredentialsHandler& operator=(const DigitalCredentialsHandler&) =
      delete;

  // DevToolsDomainHandler:
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  void Wire(UberDispatcher* dispatcher) override;

  // DigitalCredentials::Backend:
  // |in_protocol| and |in_response| are required when |in_action| is
  // "respond" and forbidden otherwise.
  CONTENT_EXPORT DispatchResponse SetVirtualWalletBehavior(
      const String& in_action,
      std::optional<String> in_protocol,
      std::unique_ptr<protocol::DictionaryValue> in_response,
      std::optional<String> in_frame_id) override;

 private:
  raw_ptr<RenderFrameHostImpl> frame_host_ = nullptr;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DIGITAL_CREDENTIALS_HANDLER_H_
