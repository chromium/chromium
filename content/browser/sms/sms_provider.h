// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_PROVIDER_H_
#define CONTENT_BROWSER_SMS_SMS_PROVIDER_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "content/browser/sms/sms_parser.h"
#include "content/common/content_export.h"

namespace url {
class Origin;
}

namespace content {

// This class wraps the platform-specific functions and allows tests to
// inject custom providers.
class CONTENT_EXPORT SmsProvider {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Receive an |sms| from an origin. Return true if the message is
    // handled, which stops its propagation to other observers.
    virtual bool OnReceive(const url::Origin&,
                           const std::string& one_time_code,
                           const std::string& sms) = 0;
  };

  SmsProvider();
  virtual ~SmsProvider();

  // Listen to the next incoming SMS and notify observers (exactly once) when
  // it is received or (exclusively) when it timeouts.
  virtual void Retrieve() = 0;

  static std::unique_ptr<SmsProvider> Create();

  void AddObserver(Observer*);
  void RemoveObserver(const Observer*);
  void NotifyReceive(const url::Origin&,
                     const std::string& one_time_code,
                     const std::string& sms);
  void NotifyReceive(const std::string& sms);
  bool HasObservers();

 private:
  base::ObserverList<Observer> observers_;
  DISALLOW_COPY_AND_ASSIGN(SmsProvider);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_PROVIDER_H_
