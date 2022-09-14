// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"

#import "base/check.h"
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

ChromeBrowserProvider::ChromeBrowserProvider() {}

ChromeBrowserProvider::~ChromeBrowserProvider() {
  chrome_identity_service_.reset();
  for (auto& observer : observer_list_)
    observer.OnChromeBrowserProviderWillBeDestroyed();
}

void ChromeBrowserProvider::SetChromeIdentityServiceForTesting(
    std::unique_ptr<ChromeIdentityService> service) {
  if (service && chrome_identity_service_replaced_for_testing_) {
    // If the service for testing has already been set, there is no need to
    // replace it again.
    // TODO(crbug.com/1201182): cleanup.
    return;
  }
  chrome_identity_service_replaced_for_testing_ = service.get() != nullptr;
  chrome_identity_service_ = std::move(service);
  FireChromeIdentityServiceDidChange(chrome_identity_service_.get());
}

ChromeIdentityService* ChromeBrowserProvider::GetChromeIdentityService() {
  if (!chrome_identity_service_) {
    chrome_identity_service_ = CreateChromeIdentityService();
  }
  return chrome_identity_service_.get();
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
