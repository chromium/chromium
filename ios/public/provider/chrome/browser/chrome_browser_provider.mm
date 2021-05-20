// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

#include <cstddef>

#include "components/metrics/metrics_provider.h"
#import "ios/public/provider/chrome/browser/mailto/mailto_handler_provider.h"
#import "ios/public/provider/chrome/browser/modals/modals_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#import "ios/public/provider/chrome/browser/text_zoom_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {

namespace {
ChromeBrowserProvider* g_chrome_browser_provider = nullptr;
}  // namespace

void SetChromeBrowserProvider(ChromeBrowserProvider* provider) {
  g_chrome_browser_provider = provider;
}

ChromeBrowserProvider* GetChromeBrowserProvider() {
  return g_chrome_browser_provider;
}

// A dummy implementation of ChromeBrowserProvider.

ChromeBrowserProvider::ChromeBrowserProvider()
    : mailto_handler_provider_(std::make_unique<MailtoHandlerProvider>()),
      modals_provider_(std::make_unique<ModalsProvider>()),
      text_zoom_provider_(std::make_unique<TextZoomProvider>()) {}

ChromeBrowserProvider::~ChromeBrowserProvider() {
  for (auto& observer : observer_list_)
    observer.OnChromeBrowserProviderWillBeDestroyed();
}

void ChromeBrowserProvider::AppendSwitchesFromExperimentalSettings(
    NSUserDefaults* experimental_settings,
    base::CommandLine* command_line) const {}

void ChromeBrowserProvider::Initialize() const {}

SigninErrorProvider* ChromeBrowserProvider::GetSigninErrorProvider() {
  return nullptr;
}

SigninResourcesProvider* ChromeBrowserProvider::GetSigninResourcesProvider() {
  return nullptr;
}

void ChromeBrowserProvider::SetChromeIdentityServiceForTesting(
    std::unique_ptr<ChromeIdentityService> service) {}

ChromeIdentityService* ChromeBrowserProvider::GetChromeIdentityService() {
  return nullptr;
}

ChromeTrustedVaultService*
ChromeBrowserProvider::GetChromeTrustedVaultService() {
  return nullptr;
}

std::string ChromeBrowserProvider::GetRiskData() {
  return std::string();
}

UITextField* ChromeBrowserProvider::CreateStyledTextField() const {
  return nil;
}

void ChromeBrowserProvider::AttachBrowserAgents(Browser* browser) const {}

VoiceSearchProvider* ChromeBrowserProvider::GetVoiceSearchProvider() const {
  return nullptr;
}

AppDistributionProvider* ChromeBrowserProvider::GetAppDistributionProvider()
    const {
  return nullptr;
}

id<LogoVendor> ChromeBrowserProvider::CreateLogoVendor(
    Browser* browser,
    web::WebState* web_state) const {
  return nil;
}

OmahaServiceProvider* ChromeBrowserProvider::GetOmahaServiceProvider() const {
  return nullptr;
}

UserFeedbackProvider* ChromeBrowserProvider::GetUserFeedbackProvider() const {
  return nullptr;
}

SpotlightProvider* ChromeBrowserProvider::GetSpotlightProvider() const {
  return nullptr;
}

FullscreenProvider* ChromeBrowserProvider::GetFullscreenProvider() const {
  return nullptr;
}

BrowserURLRewriterProvider*
ChromeBrowserProvider::GetBrowserURLRewriterProvider() const {
  return nullptr;
}

OverridesProvider* ChromeBrowserProvider::GetOverridesProvider() const {
  return nullptr;
}

DiscoverFeedProvider* ChromeBrowserProvider::GetDiscoverFeedProvider() const {
  return nullptr;
}

MailtoHandlerProvider* ChromeBrowserProvider::GetMailtoHandlerProvider() const {
  return mailto_handler_provider_.get();
}

BrandedImageProvider* ChromeBrowserProvider::GetBrandedImageProvider() const {
  return nullptr;
}

TextZoomProvider* ChromeBrowserProvider::GetTextZoomProvider() const {
  return text_zoom_provider_.get();
}

ModalsProvider* ChromeBrowserProvider::GetModalsProvider() const {
  return modals_provider_.get();
}

void ChromeBrowserProvider::HideModalViewStack() const {}

void ChromeBrowserProvider::LogIfModalViewsArePresented() const {}

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
