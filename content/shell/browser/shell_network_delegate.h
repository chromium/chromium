// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_NETWORK_DELEGATE_H_
#define CONTENT_SHELL_BROWSER_SHELL_NETWORK_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "net/base/network_delegate_impl.h"

namespace content {

class ShellNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  ShellNetworkDelegate();
  ~ShellNetworkDelegate() override;

  static void SetBlockThirdPartyCookies(bool block);
  static void SetCancelURLRequestWithPolicyViolatingReferrerHeader(bool cancel);

 private:
  // net::NetworkDelegate implementation.
  bool OnCanGetCookies(const net::URLRequest& request,
                       const net::CookieList& cookie_list,
                       bool allowed_from_caller) override;
  bool OnCanSetCookie(const net::URLRequest& request,
                      const net::CanonicalCookie& cookie,
                      net::CookieOptions* options,
                      bool allowed_from_caller) override;
  bool OnCanAccessFile(const net::URLRequest& request,
                       const base::FilePath& original_path,
                       const base::FilePath& absolute_path) const override;
  bool OnCancelURLRequestWithPolicyViolatingReferrerHeader(
      const net::URLRequest& request,
      const GURL& target_url,
      const GURL& referrer_url) const override;

  DISALLOW_COPY_AND_ASSIGN(ShellNetworkDelegate);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_NETWORK_DELEGATE_H_
