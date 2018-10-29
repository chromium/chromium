// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_network_delegate.h"

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "content/public/common/content_switches.h"
#include "net/base/net_errors.h"
#include "net/base/static_cookie_policy.h"
#include "net/url_request/url_request.h"

namespace content {

namespace {
bool g_block_third_party_cookies = false;
bool g_cancel_requests_with_referrer_policy_violation = false;
}

ShellNetworkDelegate::ShellNetworkDelegate() {
}

ShellNetworkDelegate::~ShellNetworkDelegate() {
}

void ShellNetworkDelegate::SetBlockThirdPartyCookies(bool block) {
  g_block_third_party_cookies = block;
}

void ShellNetworkDelegate::SetCancelURLRequestWithPolicyViolatingReferrerHeader(
    bool cancel) {
  g_cancel_requests_with_referrer_policy_violation = cancel;
}

bool ShellNetworkDelegate::OnCanGetCookies(const net::URLRequest& request,
                                           const net::CookieList& cookie_list,
                                           bool allowed_from_caller) {
  net::StaticCookiePolicy::Type policy_type = g_block_third_party_cookies ?
      net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES :
      net::StaticCookiePolicy::ALLOW_ALL_COOKIES;
  net::StaticCookiePolicy policy(policy_type);
  int rv = policy.CanAccessCookies(request.url(), request.site_for_cookies());
  return allowed_from_caller && rv == net::OK;
}

bool ShellNetworkDelegate::OnCanSetCookie(const net::URLRequest& request,
                                          const net::CanonicalCookie& cookie,
                                          net::CookieOptions* options,
                                          bool allowed_from_caller) {
  net::StaticCookiePolicy::Type policy_type = g_block_third_party_cookies ?
      net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES :
      net::StaticCookiePolicy::ALLOW_ALL_COOKIES;
  net::StaticCookiePolicy policy(policy_type);
  int rv = policy.CanAccessCookies(request.url(), request.site_for_cookies());
  return allowed_from_caller && rv == net::OK;
}

bool ShellNetworkDelegate::OnCanAccessFile(
    const net::URLRequest& request,
    const base::FilePath& original_path,
    const base::FilePath& absolute_path) const {
  return true;
}

bool ShellNetworkDelegate::OnCancelURLRequestWithPolicyViolatingReferrerHeader(
    const net::URLRequest& request,
    const GURL& target_url,
    const GURL& referrer_url) const {
  return g_cancel_requests_with_referrer_policy_violation;
}

}  // namespace content
