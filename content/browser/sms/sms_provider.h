// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_PROVIDER_H_
#define CONTENT_BROWSER_SMS_SMS_PROVIDER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "content/browser/sms/sms_parser.h"
#include "content/common/content_export.h"

namespace url {
class Origin;
}

namespace content {

class RenderFrameHost;

// This class wraps the platform-specific functions and allows tests to
// inject custom providers.
class CONTENT_EXPORT SmsProvider {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Receive an |one_time_code| from an origin. Return true if the message is
    // handled, which stops its propagation to other observers.
    virtual bool OnReceive(const url::Origin&,
                           const std::string& one_time_code) = 0;
    virtual void NotifyParsingFailure(SmsParser::SmsParsingStatus) {}
  };

  SmsProvider();
  virtual ~SmsProvider();

  // Listen to the next incoming SMS and notify observers (exactly once) when
  // it is received or (exclusively) when it timeouts. |render_frame_host|
  // is the RenderFrameHost for the renderer that issued the request, and is
  // passed in to support showing native permission confirmation prompt on the
  // relevant window.
  virtual void Retrieve(RenderFrameHost* render_frame_host) = 0;

  static std::unique_ptr<SmsProvider> Create();

  void AddObserver(Observer*);
  void RemoveObserver(const Observer*);
  void NotifyReceive(const url::Origin&, const std::string& one_time_code);
  void NotifyReceiveForTesting(const std::string& sms);
  void RecordParsingStatus(SmsParser::SmsParsingStatus status);
  bool HasObservers();

 protected:
  void NotifyReceive(const std::string& sms);

 private:
  base::ObserverList<Observer> observers_;
  DISALLOW_COPY_AND_ASSIGN(SmsProvider);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_PROVIDER_H_
