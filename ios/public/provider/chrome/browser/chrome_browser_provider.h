// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CHROME_BROWSER_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CHROME_BROWSER_PROVIDER_H_

#include <memory>

#include "base/observer_list.h"

namespace ios {

class ChromeBrowserProvider;
class ChromeIdentityService;

// Getter and setter for the provider. The provider should be set early, before
// any browser code is called (as the getter will fail if the provider has not
// been set).
ChromeBrowserProvider& GetChromeBrowserProvider();
[[nodiscard]] ChromeBrowserProvider* SetChromeBrowserProvider(
    ChromeBrowserProvider* provider);

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

  // Sets the current instance of Chrome identity service. Used for testing.
  void SetChromeIdentityServiceForTesting(
      std::unique_ptr<ChromeIdentityService> service);
  // Returns an instance of a Chrome identity service.
  ChromeIdentityService* GetChromeIdentityService();

  // Adds and removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  // Fires `OnChromeIdentityServiceDidChange` on all observers.
  void FireChromeIdentityServiceDidChange(ChromeIdentityService* new_service);

  // Creates a ChromeIdentityService. This methods has to be be implemented
  // in subclasses.
  virtual std::unique_ptr<ios::ChromeIdentityService>
  CreateChromeIdentityService() = 0;

 private:
  base::ObserverList<Observer, true>::Unchecked observer_list_;
  std::unique_ptr<ios::ChromeIdentityService> chrome_identity_service_;
  bool chrome_identity_service_replaced_for_testing_ = false;
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CHROME_BROWSER_PROVIDER_H_
