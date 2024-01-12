// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_WEBOTP_SERVICE_H_
#define CONTENT_BROWSER_SMS_WEBOTP_SERVICE_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/sms/sms_parser.h"
#include "content/browser/sms/sms_queue.h"
#include "content/browser/sms/user_consent_handler.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/sms/webotp_service.mojom.h"
#include "url/origin.h"

namespace content {

class RenderFrameHost;
class SmsFetcher;

// WebOTPService handles mojo connections from the renderer, observing the
// incoming SMS messages from an SmsFetcher. In practice, it is owned and
// managed by a RenderFrameHost. It accomplishes that via subclassing
// DocumentService, which observes the lifecycle of a RenderFrameHost and
// manages it own memory. Create() creates a self-managed instance of
// WebOTPService and binds it to the request.
class CONTENT_EXPORT WebOTPService
    : public DocumentService<blink::mojom::WebOTPService>,
      public SmsFetcher::Subscriber {
 public:
  // Return value indicates success. Creation can fail if origin requirements
  // are not met.
  static bool Create(SmsFetcher*,
                     RenderFrameHost*,
                     mojo::PendingReceiver<blink::mojom::WebOTPService>);
  static WebOTPService& CreateForTesting(
      SmsFetcher*,
      const OriginList&,
      RenderFrameHost&,
      mojo::PendingReceiver<blink::mojom::WebOTPService>);

  WebOTPService(const WebOTPService&) = delete;
  WebOTPService& operator=(const WebOTPService&) = delete;

  ~WebOTPService() override;

  using FailureType = SmsFetchFailureType;
  using SmsParsingStatus = SmsParser::SmsParsingStatus;
  using UserConsent = SmsFetcher::UserConsent;

  // content::DocumentService:
  void WillBeDestroyed(DocumentServiceDestructionReason) override;

  // blink::mojom::WebOTPService:
  void Receive(ReceiveCallback) override;
  void Abort() override;

  // content::SmsQueue::Subscriber
  void OnReceive(const OriginList&,
                 const std::string& one_time_code,
                 UserConsent) override;
  void OnFailure(FailureType failure_type) override;

  // Completes the in-flight sms otp code request. Invokes the receive callback,
  // if one is available, with the provided status code and the existing one
  // time code.
  void CompleteRequest(blink::mojom::SmsStatus);
  void SetConsentHandlerForTesting(UserConsentHandler*);

  // Rejects the pending request if it has not been resolved naturally yet.
  void OnTimeout();

  void OnUserConsentComplete(UserConsentResult);

 private:
  WebOTPService(SmsFetcher*,
                const OriginList&,
                RenderFrameHost&,
                mojo::PendingReceiver<blink::mojom::WebOTPService>);

  void CleanUp();
  UserConsentHandler* CreateConsentHandler(UserConsent);
  UserConsentHandler* GetConsentHandler();
  void RecordMetrics(blink::mojom::SmsStatus);

  // |fetcher_| is safe because all instances of SmsFetcher are owned
  // by the browser context, which transitively (through RenderFrameHost) owns
  // and outlives this class.
  raw_ptr<SmsFetcher> fetcher_;

  const OriginList origin_list_;
  ReceiveCallback callback_;
  std::optional<std::string> one_time_code_;
  base::TimeTicks start_time_;
  base::TimeTicks receive_time_;
  // Timer to trigger timeout for any pending request. We (re)arm the timer
  // every time we receive a new request.
  base::DelayTimer timeout_timer_;
  std::optional<FailureType> delayed_rejection_reason_;

  // The ptr is valid only when we are handling an incoming otp response.
  std::unique_ptr<UserConsentHandler> consent_handler_;
  // This is used to inject a mock consent handler for testing and it is owned
  // by test code.
  raw_ptr<UserConsentHandler> consent_handler_for_test_{nullptr};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WebOTPService> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_WEBOTP_SERVICE_H_
