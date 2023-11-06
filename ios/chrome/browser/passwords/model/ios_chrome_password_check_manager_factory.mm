// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"

#import "base/memory/ref_counted.h"
#import "base/memory/weak_ptr.h"
#import "base/no_destructor.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_bulk_leak_check_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

namespace {

class IOSChromePasswordCheckManagerProxy : public KeyedService {
 public:
  explicit IOSChromePasswordCheckManagerProxy(ChromeBrowserState* browser_state)
      : browser_state_(browser_state) {}

  void Shutdown() override { browser_state_ = nullptr; }

  scoped_refptr<IOSChromePasswordCheckManager> GetOrCreateManager() {
    if (instance_) {
      return scoped_refptr<IOSChromePasswordCheckManager>(instance_.get());
    }

    scoped_refptr<IOSChromePasswordCheckManager> manager =
        new IOSChromePasswordCheckManager(
            IOSChromeProfilePasswordStoreFactory::GetForBrowserState(
                browser_state_, ServiceAccessType::EXPLICIT_ACCESS),
            IOSChromeAccountPasswordStoreFactory::GetForBrowserState(
                browser_state_, ServiceAccessType::EXPLICIT_ACCESS),
            IOSChromeAffiliationServiceFactory::GetForBrowserState(
                browser_state_),
            IOSChromeBulkLeakCheckServiceFactory::GetForBrowserState(
                browser_state_),
            browser_state_->GetPrefs());

    instance_ = manager->AsWeakPtr();

    return manager;
  }

 private:
  ChromeBrowserState* browser_state_ = nullptr;
  base::WeakPtr<IOSChromePasswordCheckManager> instance_;
};
}  // namespace

// static
IOSChromePasswordCheckManagerFactory*
IOSChromePasswordCheckManagerFactory::GetInstance() {
  static base::NoDestructor<IOSChromePasswordCheckManagerFactory> instance;
  return instance.get();
}

// static
scoped_refptr<IOSChromePasswordCheckManager>
IOSChromePasswordCheckManagerFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<IOSChromePasswordCheckManagerProxy*>(
             GetInstance()->GetServiceForBrowserState(browser_state, true))
      ->GetOrCreateManager();
}

IOSChromePasswordCheckManagerFactory::IOSChromePasswordCheckManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "PasswordCheckManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IOSChromeProfilePasswordStoreFactory::GetInstance());
  DependsOn(IOSChromeAccountPasswordStoreFactory::GetInstance());
  DependsOn(IOSChromeAffiliationServiceFactory::GetInstance());
  DependsOn(IOSChromeBulkLeakCheckServiceFactory::GetInstance());
}

IOSChromePasswordCheckManagerFactory::~IOSChromePasswordCheckManagerFactory() =
    default;

std::unique_ptr<KeyedService>
IOSChromePasswordCheckManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<IOSChromePasswordCheckManagerProxy>(
      ChromeBrowserState::FromBrowserState(context));
}
