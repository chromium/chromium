// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_MODEL_IOS_CHROME_NETWORK_DELEGATE_H_
#define IOS_CHROME_BROWSER_NET_MODEL_IOS_CHROME_NETWORK_DELEGATE_H_

#include <stdint.h>

#import "base/memory/raw_ptr.h"
#include "net/base/network_delegate_impl.h"

class PrefService;

template <typename T>
class PrefMember;

typedef PrefMember<bool> BooleanPrefMember;

// IOSChromeNetworkDelegate is the central point from within the Chrome code to
// add hooks into the network stack.
class IOSChromeNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  IOSChromeNetworkDelegate();

  IOSChromeNetworkDelegate(const IOSChromeNetworkDelegate&) = delete;
  IOSChromeNetworkDelegate& operator=(const IOSChromeNetworkDelegate&) = delete;

  ~IOSChromeNetworkDelegate() override;

 private:
  bool OnCancelURLRequestWithPolicyViolatingReferrerHeader(
      const net::URLRequest& request,
      const GURL& target_url,
      const GURL& referrer_url) const override;
};

#endif  // IOS_CHROME_BROWSER_NET_MODEL_IOS_CHROME_NETWORK_DELEGATE_H_
