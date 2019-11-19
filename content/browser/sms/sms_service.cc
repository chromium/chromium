// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_service.h"

#include <iterator>
#include <queue>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/optional.h"
#include "content/browser/sms/sms_metrics.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

using blink::mojom::SmsStatus;

namespace content {

SmsService::SmsService(
    SmsFetcher* fetcher,
    const url::Origin& origin,
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::SmsReceiver> receiver)
    : FrameServiceBase(host, std::move(receiver)),
      fetcher_(fetcher),
      origin_(origin) {}

SmsService::SmsService(
    SmsFetcher* fetcher,
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::SmsReceiver> receiver)
    : SmsService(fetcher,
                 host->GetLastCommittedOrigin(),
                 host,
                 std::move(receiver)) {}

SmsService::~SmsService() {
  if (callback_)
    Process(SmsStatus::kTimeout, base::nullopt);
}

// static
void SmsService::Create(
    SmsFetcher* fetcher,
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::SmsReceiver> receiver) {
  DCHECK(host);

  // SmsService owns itself. It will self-destruct when a mojo interface
  // error occurs, the render frame host is deleted, or the render frame host
  // navigates to a new document.
  new SmsService(fetcher, host, std::move(receiver));
}

void SmsService::Receive(ReceiveCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (callback_) {
    std::move(callback_).Run(SmsStatus::kCancelled, base::nullopt);
    fetcher_->Unsubscribe(origin_, this);
  }

  start_time_ = base::TimeTicks::Now();

  callback_ = std::move(callback);

  fetcher_->Subscribe(origin_, this);
}

void SmsService::OnReceive(const std::string& one_time_code,
                           const std::string& sms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!sms_);
  DCHECK(!start_time_.is_null());

  RecordSmsReceiveTime(base::TimeTicks::Now() - start_time_);

  sms_ = sms;
  receive_time_ = base::TimeTicks::Now();

  OpenInfoBar(one_time_code);
}

void SmsService::OpenInfoBar(const std::string& one_time_code) {
  WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host());

  web_contents->GetDelegate()->CreateSmsPrompt(
      render_frame_host(), origin_, one_time_code,
      base::BindOnce(&SmsService::OnConfirm, weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&SmsService::OnCancel, weak_ptr_factory_.GetWeakPtr()));
}

void SmsService::Process(blink::mojom::SmsStatus status,
                         base::Optional<std::string> sms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(callback_);

  std::move(callback_).Run(status, sms);

  CleanUp();
}

void SmsService::OnConfirm() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(sms_);
  DCHECK(!receive_time_.is_null());
  RecordContinueOnSuccessTime(base::TimeTicks::Now() - receive_time_);

  Process(SmsStatus::kSuccess, sms_);
}

void SmsService::OnCancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Record only when SMS has already been received.
  DCHECK(!receive_time_.is_null());
  RecordCancelOnSuccessTime(base::TimeTicks::Now() - receive_time_);

  Process(SmsStatus::kCancelled, base::nullopt);
}

void SmsService::CleanUp() {
  callback_.Reset();
  sms_.reset();
  start_time_ = base::TimeTicks();
  receive_time_ = base::TimeTicks();
  fetcher_->Unsubscribe(origin_, this);
}

}  // namespace content
