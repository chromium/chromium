// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_SERVICE_H_
#define CONTENT_BROWSER_SMS_SMS_SERVICE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/browser/sms/sms_queue.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_service_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom.h"
#include "url/origin.h"

namespace content {

class RenderFrameHost;
class SmsFetcher;

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
             const url::Origin&,
             RenderFrameHost*,
             mojo::PendingReceiver<blink::mojom::SmsReceiver>);
  ~SmsService() override;

  // blink::mojom::SmsReceiver:
  void Receive(ReceiveCallback) override;

  // content::SmsQueue::Subscriber
  void OnReceive(const std::string& one_time_code,
                 const std::string& sms) override;

 private:
  void OpenInfoBar(const std::string& one_time_code);
  void Process(blink::mojom::SmsStatus, base::Optional<std::string> sms);
  void CleanUp();

  // Called when the user manually clicks the 'Enter code' button.
  void OnConfirm();
  // Called when the user manually dismisses the infobar.
  void OnCancel();

  // |fetcher_| is safe because all instances of SmsFetcher are owned
  // by the browser context, which transitively (through RenderFrameHost) owns
  // and outlives this class.
  SmsFetcher* fetcher_;

  const url::Origin origin_;

  ReceiveCallback callback_;
  base::Optional<std::string> sms_;
  base::TimeTicks start_time_;
  base::TimeTicks receive_time_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SmsService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SmsService);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_SERVICE_H_
