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

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"

class AppDistributionProvider;
class BrandedImageProvider;
class BrowserURLRewriterProvider;
class FullscreenProvider;
class MailtoHandlerProvider;
class OmahaServiceProvider;
class OverridesProvider;
class SpotlightProvider;
class UserFeedbackProvider;
class VoiceSearchProvider;

namespace base {
class CommandLine;
}

namespace web {
class SerializableUserDataManager;
class WebState;
}

class GURL;
@protocol LogoVendor;
@class TabModel;
@class UITextField;
@class UIView;

namespace ios {

class ChromeBrowserProvider;
class ChromeBrowserState;
class ChromeIdentityService;
class GeolocationUpdaterProvider;
class SigninErrorProvider;
class SigninResourcesProvider;

// Setter and getter for the provider. The provider should be set early, before
// any browser code is called.
void SetChromeBrowserProvider(ChromeBrowserProvider* provider);
ChromeBrowserProvider* GetChromeBrowserProvider();

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
    virtual ~Observer() {}

    // Called when a new ChromeIdentityService has been installed.
    virtual void OnChromeIdentityServiceDidChange(
        ChromeIdentityService* new_service) {}

    // Called when the ChromeBrowserProvider will be destroyed.
    virtual void OnChromeBrowserProviderWillBeDestroyed() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
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

  // Returns an instance of a signing error provider.
  virtual SigninErrorProvider* GetSigninErrorProvider();
  // Returns an instance of a signin resources provider.
  virtual SigninResourcesProvider* GetSigninResourcesProvider();
  // Sets the current instance of Chrome identity service. Used for testing.
  virtual void SetChromeIdentityServiceForTesting(
      std::unique_ptr<ChromeIdentityService> service);
  // Returns an instance of a Chrome identity service.
  virtual ChromeIdentityService* GetChromeIdentityService();
  // Returns an instance of a GeolocationUpdaterProvider.
  virtual GeolocationUpdaterProvider* GetGeolocationUpdaterProvider();
  // Returns risk data used in Wallet requests.
  virtual std::string GetRiskData();
  // Creates and returns a new styled text field.
  virtual UITextField* CreateStyledTextField() const NS_RETURNS_RETAINED;
  // Allow embedders to inject data.
  virtual void AddSerializableData(
      web::SerializableUserDataManager* user_data_manager,
      web::WebState* web_state);
  // Allow embedders to block a specific URL.
  virtual bool ShouldBlockUrlDuringRestore(const GURL& url,
                                           web::WebState* web_state);

  // Initializes the cast service.  Should be called soon after the given
  // |main_tab_model| is created.
  virtual void InitializeCastService(TabModel* main_tab_model) const;

  // Attaches any embedder-specific tab helpers to the given |web_state|.
  virtual void AttachTabHelpers(web::WebState* web_state) const;

  // Returns an instance of the voice search provider, if one exists.
  virtual VoiceSearchProvider* GetVoiceSearchProvider() const;

  // Returns an instance of the app distribution provider.
  virtual AppDistributionProvider* GetAppDistributionProvider() const;

  // Creates and returns an object that can fetch and vend search engine logos.
  // The caller assumes ownership of the returned object.
  virtual id<LogoVendor> CreateLogoVendor(
      ios::ChromeBrowserState* browser_state) const NS_RETURNS_RETAINED;

  // Returns an instance of the omaha service provider.
  virtual OmahaServiceProvider* GetOmahaServiceProvider() const;

  // Returns an instance of the user feedback provider.
  virtual UserFeedbackProvider* GetUserFeedbackProvider() const;

  // Returns an instance of the branded image provider.
  virtual BrandedImageProvider* GetBrandedImageProvider() const;

  // Hides immediately the modals related to this provider.
  virtual void HideModalViewStack() const;

  // Logs if any modals created by this provider are still presented. It does
  // not dismiss them.
  virtual void LogIfModalViewsArePresented() const;

  // Returns an instance of the spotlight provider.
  virtual SpotlightProvider* GetSpotlightProvider() const;

  // Returns a valid non-null instance of the mailto handler provider.
  virtual MailtoHandlerProvider* GetMailtoHandlerProvider() const;

  // Returns an instance of the fullscreen provider.
  virtual FullscreenProvider* GetFullscreenProvider() const;

  // Returns an instance of the BrowserURLRewriter provider.
  virtual BrowserURLRewriterProvider* GetBrowserURLRewriterProvider() const;

  // Returns an instance of the Overrides provider;
  virtual OverridesProvider* GetOverridesProvider() const;

  // Adds and removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  // Fires |OnChromeIdentityServiceDidChange| on all observers.
  void FireChromeIdentityServiceDidChange(ChromeIdentityService* new_service);

 private:
  base::ObserverList<Observer, true>::Unchecked observer_list_;
  std::unique_ptr<MailtoHandlerProvider> mailto_handler_provider_;
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CHROME_BROWSER_PROVIDER_H_
