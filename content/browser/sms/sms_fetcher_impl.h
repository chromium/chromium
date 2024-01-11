// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_FETCHER_IMPL_H_
#define CONTENT_BROWSER_SMS_SMS_FETCHER_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "content/browser/sms/sms_provider.h"
#include "content/browser/sms/sms_queue.h"
#include "content/common/content_export.h"
#include "content/public/browser/sms_fetcher.h"
#include "url/origin.h"

namespace content {

class SmsProvider;

// SmsFetcherImpls coordinate between local and remote SMS providers as well as
// between multiple origins. There is one SmsFetcherImpl per profile. An
// instance must only be used on the sequence it was created on.
class CONTENT_EXPORT SmsFetcherImpl : public content::SmsFetcher,
                                      public base::SupportsUserData::Data,
                                      public SmsProvider::Observer {
 public:
  explicit SmsFetcherImpl(SmsProvider* provider);
  using FailureType = SmsFetchFailureType;

  SmsFetcherImpl(const SmsFetcherImpl&) = delete;
  SmsFetcherImpl& operator=(const SmsFetcherImpl&) = delete;

  ~SmsFetcherImpl() override;

  // Called by devices that do not have telephony capabilities and exclusively
  // listen for SMSes received on other devices.
  void Subscribe(const OriginList& origin_list,
                 Subscriber& subscriber) override;
  // Called by |WebOTPService| to fetch SMSes retrieved by the SmsProvider from
  // the requested device.
  void Subscribe(const OriginList& origin_list,
                 Subscriber& subscriber,
                 RenderFrameHost& rfh) override;
  void Unsubscribe(const OriginList& origin_list,
                   Subscriber* subscriber) override;

  // content::SmsProvider::Observer:
  bool OnReceive(const OriginList& origin_list,
                 const std::string& one_time_code,
                 UserConsent) override;
  bool OnFailure(FailureType failure_type) override;

  bool HasSubscribers() override;

 private:
  void OnRemote(std::optional<OriginList>,
                std::optional<std::string> one_time_code,
                std::optional<FailureType> failure_type);

  bool Notify(const OriginList& origin_list,
              const std::string& one_time_code,
              UserConsent);

  // |provider_| is safe because all instances of SmsProvider are owned
  // by the BrowserMainLoop, which outlive instances of this class.
  const raw_ptr<SmsProvider> provider_;

  SmsQueue subscribers_;
  // A cancel callback can cancel receiving of the remote fetching response.
  // Calling it will run a response callback if it hasn't been executed yet,
  // otherwise it will be a no-op. The response callback can clear the sharing
  // states including the UI element in the omnibox.
  base::flat_map<Subscriber*, base::OnceClosure> remote_cancel_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SmsFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_FETCHER_IMPL_H_
