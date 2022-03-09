// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

#include <cstddef>

#include "base/check.h"
#include "components/metrics/metrics_provider.h"
#import "ios/public/provider/chrome/browser/mailto/mailto_handler_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {

namespace {
ChromeBrowserProvider* g_chrome_browser_provider = nullptr;
}  // namespace

ChromeBrowserProvider* SetChromeBrowserProvider(
    ChromeBrowserProvider* provider) {
  ChromeBrowserProvider* previous = g_chrome_browser_provider;
  g_chrome_browser_provider = provider;
  return previous;
}

ChromeBrowserProvider& GetChromeBrowserProvider() {
  DCHECK(g_chrome_browser_provider)
      << "Calling GetChromeBrowserProvider() before SetChromeBrowserProvider()";
  return *g_chrome_browser_provider;
}

// A dummy implementation of ChromeBrowserProvider.

ChromeBrowserProvider::ChromeBrowserProvider()
    : mailto_handler_provider_(std::make_unique<MailtoHandlerProvider>()) {}

ChromeBrowserProvider::~ChromeBrowserProvider() {
  chrome_identity_service_.reset();
  for (auto& observer : observer_list_)
    observer.OnChromeBrowserProviderWillBeDestroyed();
}

void ChromeBrowserProvider::SetChromeIdentityServiceForTesting(
    std::unique_ptr<ChromeIdentityService> service) {
  chrome_identity_service_ = std::move(service);
  FireChromeIdentityServiceDidChange(chrome_identity_service_.get());
}

ChromeIdentityService* ChromeBrowserProvider::GetChromeIdentityService() {
  if (!chrome_identity_service_) {
    chrome_identity_service_ = CreateChromeIdentityService();
  }
  return chrome_identity_service_.get();
}

ChromeTrustedVaultService*
ChromeBrowserProvider::GetChromeTrustedVaultService() {
  return nullptr;
}

UserFeedbackProvider* ChromeBrowserProvider::GetUserFeedbackProvider() const {
  return nullptr;
}

FollowProvider* ChromeBrowserProvider::GetFollowProvider() const {
  return nullptr;
}

MailtoHandlerProvider* ChromeBrowserProvider::GetMailtoHandlerProvider() const {
  return mailto_handler_provider_.get();
}

void ChromeBrowserProvider::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ChromeBrowserProvider::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ChromeBrowserProvider::FireChromeIdentityServiceDidChange(
    ChromeIdentityService* new_service) {
  for (auto& observer : observer_list_)
    observer.OnChromeIdentityServiceDidChange(new_service);
}

}  // namespace ios
