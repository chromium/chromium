// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_SERVICE_H_
#define CONTENT_BROWSER_SMS_SMS_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/browser/sms/sms_queue.h"
#include "content/browser/sms/user_consent_handler.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_service_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom.h"
#include "url/origin.h"

namespace content {

class RenderFrameHost;
class SmsFetcher;
struct LoadCommittedDetails;

// SmsService handles mojo connections from the renderer, observing the incoming
// SMS messages from an SmsFetcher.
// In practice, it is owned and managed by a RenderFrameHost. It accomplishes
// that via subclassing FrameServiceBase, which observes the lifecycle of a
// RenderFrameHost and manages it own memory.
// Create() creates a self-managed instance of SmsService and binds it to the
// request.
class CONTENT_EXPORT SmsService
    : public FrameServiceBase<blink::mojom::SmsReceiver>,
      public SmsFetcher::Subscriber {
 public:
  static void Create(SmsFetcher*,
                     RenderFrameHost*,
                     mojo::PendingReceiver<blink::mojom::SmsReceiver>);

  SmsService(SmsFetcher*,
             RenderFrameHost*,
             mojo::PendingReceiver<blink::mojom::SmsReceiver>);
  SmsService(SmsFetcher*,
             std::unique_ptr<UserConsentHandler> consent_handler,
             const url::Origin&,
             RenderFrameHost*,
             mojo::PendingReceiver<blink::mojom::SmsReceiver>);
  ~SmsService() override;

  using FailureType = SmsFetcher::FailureType;
  using SmsParsingStatus = SmsParser::SmsParsingStatus;

  // blink::mojom::SmsReceiver:
  void Receive(ReceiveCallback) override;
  void Abort() override;

  // content::SmsQueue::Subscriber
  void OnReceive(const std::string& one_time_code) override;
  void OnFailure(FailureType failure_type) override;

  // Completes the in-flight sms otp code request. Invokes the receive callback,
  // if one is available, with the provided status code and the existing one
  // time code.
  void CompleteRequest(blink::mojom::SmsStatus);

 protected:
  // content::WebContentsObserver:
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;

 private:
  void CleanUp();


  // |fetcher_| is safe because all instances of SmsFetcher are owned
  // by the browser context, which transitively (through RenderFrameHost) owns
  // and outlives this class.
  SmsFetcher* fetcher_;
  std::unique_ptr<UserConsentHandler> consent_handler_;

  const url::Origin origin_;
  ReceiveCallback callback_;
  base::Optional<std::string> one_time_code_;
  base::TimeTicks start_time_;
  base::TimeTicks receive_time_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SmsService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SmsService);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_SERVICE_H_
