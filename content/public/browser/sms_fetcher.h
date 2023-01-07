// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SMS_FETCHER_H_
#define CONTENT_PUBLIC_BROWSER_SMS_FETCHER_H_

#include <string>
#include <vector>

#include "base/observer_list_types.h"
#include "content/common/content_export.h"

namespace url {
class Origin;
}

namespace content {

class BrowserContext;
class RenderFrameHost;

enum class SmsFetchFailureType {
  kNoFailure = 0,
  kSmsNotParsed_OTPFormatRegexNotMatch = 1,
  kSmsNotParsed_HostAndPortNotParsed = 2,
  kSmsNotParsed_kGURLNotValid = 3,

  kPromptTimeout = 4,
  kPromptCancelled = 5,

  // The underlying API is not available
  kBackendNotAvailable = 6,

  // Failure type when using WebOTP on desktop
  kCrossDeviceFailure = 7,

  kMaxValue = kCrossDeviceFailure,
};

// TODO(yigu): Do not use anonymous namespace in header.
namespace {
using OriginList = std::vector<url::Origin>;
}  // namespace

// SmsFetcher coordinates between the provisioning of SMSes coming from the
// local device or remote devices to multiple origins.
// There is one SmsFetcher per profile.
class SmsFetcher {
 public:
  // Indicates whether the subscriber needs to obtain its own user consent or
  // not.
  enum class UserConsent {
    // The fetcher has not obtained the user consent to share the OTP. It is
    // expected that the the subscriber (the browser) will do so.
    kNotObtained,
    // The fetcher has already obtained the user consent to share the OTP.
    // Currently this is the case only when GMS User Consent API is used.
    kObtained
  };

  SmsFetcher() = default;
  virtual ~SmsFetcher() = default;

  // Retrieval for devices that exclusively listen for SMSes coming from other
  // telephony devices. (eg. desktop)
  CONTENT_EXPORT static SmsFetcher* Get(BrowserContext* context);

  class Subscriber : public base::CheckedObserver {
   public:
    // Receive an |origin_list| and a |one_time_code| from subscribed origin.
    // The |origin_list| is for verification purpose on remote device and the
    // |one_time_code| is used as an alphanumeric value which the origin uses to
    // verify the ownership of the phone number.
    virtual void OnReceive(const OriginList& origin_list,
                           const std::string& one_time_code,
                           UserConsent) = 0;
    virtual void OnFailure(SmsFetchFailureType failure_type) = 0;
  };

  // Subscribes to incoming SMSes from SmsProvider for subscribers that do not
  // have telephony capabilities and exclusively listen for SMSes received
  // on other devices.
  virtual void Subscribe(const OriginList& origin_list,
                         Subscriber& subscriber) = 0;
  // Subscribes to incoming SMSes from SmsProvider for telephony
  // devices that can receive SMSes locally and can show a permission prompt.
  // TODO(yigu): This API is used in content/ only. We should move it to the
  // SmsFetcherImpl per guideline. https://crbug.com/1136062.
  virtual void Subscribe(const OriginList& origin_list,
                         Subscriber& subscriber,
                         RenderFrameHost& render_frame_host) = 0;
  virtual void Unsubscribe(const OriginList& origin_list,
                           Subscriber* subscriber) = 0;
  // TODO(yigu): This API is used in content/ only. We should move it to the
  // SmsFetcherImpl per guideline. https://crbug.com/1136062.
  virtual bool HasSubscribers() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SMS_FETCHER_H_
