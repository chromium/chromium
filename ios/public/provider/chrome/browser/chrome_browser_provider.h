// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CHROME_BROWSER_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CHROME_BROWSER_PROVIDER_H_

#include <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"

class DiscoverFeedProvider;
class FollowProvider;
class MailtoHandlerProvider;
class UserFeedbackProvider;

namespace base {
class CommandLine;
}

namespace web {
class WebState;
}

@protocol LogoVendor;
@class UITextField;
@class UIView;
class Browser;

namespace ios {

class ChromeBrowserProvider;
class ChromeIdentityService;
class ChromeTrustedVaultService;

// Getter and setter for the provider. The provider should be set early, before
// any browser code is called (as the getter will fail if the provider has not
// been set).
ChromeBrowserProvider& GetChromeBrowserProvider();
ChromeBrowserProvider* SetChromeBrowserProvider(ChromeBrowserProvider* provider)
    WARN_UNUSED_RESULT;

// Factory function for the embedder specific provider. This function must be
// implemented by the embedder and will be selected via linking (i.e. by the
// build system). Should only be used in the application startup code, not by
// the tests (as they may use a different provider).
std::unique_ptr<ChromeBrowserProvider> CreateChromeBrowserProvider();

// A class that allows embedding iOS-specific functionality in the
// ios_chrome_browser target.
class ChromeBrowserProvider {
 public:
  // Observer handling events related to the ChromeBrowserProvider.
  class Observer {
   public:
    Observer() {}

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    virtual ~Observer() {}

    // Called when a new ChromeIdentityService has been installed.
    virtual void OnChromeIdentityServiceDidChange(
        ChromeIdentityService* new_service) {}

    // Called when the ChromeBrowserProvider will be destroyed.
    virtual void OnChromeBrowserProviderWillBeDestroyed() {}
  };

  // The constructor is called before web startup.
  ChromeBrowserProvider();
  virtual ~ChromeBrowserProvider();

  // Appends additional command-line flags. Called before web startup.
  virtual void AppendSwitchesFromExperimentalSettings(
      NSUserDefaults* experimental_settings,
      base::CommandLine* command_line) const;

  // This is called after web startup.
  virtual void Initialize() const;

  // Sets the current instance of Chrome identity service. Used for testing.
  void SetChromeIdentityServiceForTesting(
      std::unique_ptr<ChromeIdentityService> service);
  // Returns an instance of a Chrome identity service.
  ChromeIdentityService* GetChromeIdentityService();
  // Returns an instance of a Chrome trusted vault service.
  virtual ChromeTrustedVaultService* GetChromeTrustedVaultService();
  // Creates and returns a new styled text field.
  virtual UITextField* CreateStyledTextField() const NS_RETURNS_RETAINED;

  // Attaches any embedder-specific browser agents to the given |browser|.
  virtual void AttachBrowserAgents(Browser* browser) const;

  virtual id<LogoVendor> CreateLogoVendor(Browser* browser,
                                          web::WebState* web_state) const
      NS_RETURNS_RETAINED;

  // Returns an instance of the user feedback provider.
  virtual UserFeedbackProvider* GetUserFeedbackProvider() const;

  // Hides immediately the modals related to this provider.
  virtual void HideModalViewStack() const;

  // Logs if any modals created by this provider are still presented. It does
  // not dismiss them.
  virtual void LogIfModalViewsArePresented() const;

  // Returns a valid non-null instance of the mailto handler provider.
  virtual MailtoHandlerProvider* GetMailtoHandlerProvider() const;

  // Returns an instance of the DiscoverFeed provider;
  virtual DiscoverFeedProvider* GetDiscoverFeedProvider() const;

  // Returns an instance of the Follow provider;
  virtual FollowProvider* GetFollowProvider() const;

  // Adds and removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  // Fires |OnChromeIdentityServiceDidChange| on all observers.
  void FireChromeIdentityServiceDidChange(ChromeIdentityService* new_service);

  // Creates a ChromeIdentityService. This methods has to be be implemented
  // in subclasses.
  virtual std::unique_ptr<ios::ChromeIdentityService>
  CreateChromeIdentityService() = 0;

 private:
  base::ObserverList<Observer, true>::Unchecked observer_list_;
  std::unique_ptr<MailtoHandlerProvider> mailto_handler_provider_;
  std::unique_ptr<ios::ChromeIdentityService> chrome_identity_service_;
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CHROME_BROWSER_PROVIDER_H_
