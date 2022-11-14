// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_PROVIDER_H_
#define CONTENT_BROWSER_SMS_SMS_PROVIDER_H_

#include <memory>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "content/browser/sms/sms_parser.h"
#include "content/common/content_export.h"

namespace content {

class RenderFrameHost;

// When WebOTP API is called on mobile, a local |SmsProvider| fetches the SMS on
// the same device. When the API is called on desktop, a remote |SmsProvider|
// will fetch the SMS on mobile. In this case we only try to use the CodeBrowser
// backend and not fall back to the UserConsent one.
enum class SmsFetchType {
  kLocal,
  kRemote,
};

// This class wraps the platform-specific functions and allows tests to
// inject custom providers.
class CONTENT_EXPORT SmsProvider {
 public:
  using FailureType = SmsFetchFailureType;
  using SmsParsingStatus = SmsParser::SmsParsingStatus;
  using UserConsent = SmsFetcher::UserConsent;

  class Observer : public base::CheckedObserver {
   public:
    // Receive an |one_time_code| from an origin. Return true if the message is
    // handled, which stops its propagation to other observers.
    virtual bool OnReceive(const OriginList&,
                           const std::string& one_time_code,
                           UserConsent) = 0;
    virtual bool OnFailure(FailureType failure_type) = 0;
    virtual void NotifyParsingFailure(SmsParser::SmsParsingStatus) {}
  };

  SmsProvider();

  SmsProvider(const SmsProvider&) = delete;
  SmsProvider& operator=(const SmsProvider&) = delete;

  virtual ~SmsProvider();

  // Listen to the next incoming SMS and notify observers (exactly once) when
  // it is received or (exclusively) when it timeouts. |render_frame_host|
  // is the RenderFrameHost for the renderer that issued the request, and is
  // passed in to support showing native permission confirmation prompt on the
  // relevant window. |fetch_type| indicates that whether the retrieval request
  // is made from a remote device, e.g. desktop.
  virtual void Retrieve(RenderFrameHost* render_frame_host,
                        SmsFetchType fetch_type) = 0;

  static std::unique_ptr<SmsProvider> Create();

  void AddObserver(Observer*);
  void RemoveObserver(const Observer*);
  void NotifyReceive(const OriginList&,
                     const std::string& one_time_code,
                     UserConsent);
  void NotifyReceiveForTesting(const std::string& sms, UserConsent);
  void NotifyFailure(FailureType failure_type);
  void RecordParsingStatus(SmsParsingStatus status);
  bool HasObservers();

 protected:
  void NotifyReceive(const std::string& sms, UserConsent);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_PROVIDER_H_
