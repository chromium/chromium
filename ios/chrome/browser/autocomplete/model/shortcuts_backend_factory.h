// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_SHORTCUTS_BACKEND_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_SHORTCUTS_BACKEND_FACTORY_H_

#import "base/memory/ref_counted.h"
#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/refcounted_profile_keyed_service_factory_ios.h"

class ProfileIOS;
class ShortcutsBackend;

namespace ios {
// Singleton that owns all ShortcutsBackends and associates them with
// ProfileIOS.
class ShortcutsBackendFactory : public RefcountedProfileKeyedServiceFactoryIOS {
 public:
  static scoped_refptr<ShortcutsBackend> GetForProfile(ProfileIOS* profile);
  static scoped_refptr<ShortcutsBackend> GetForProfileIfExists(
      ProfileIOS* profile);
  static ShortcutsBackendFactory* GetInstance();
  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<ShortcutsBackendFactory>;

  ShortcutsBackendFactory();
  ~ShortcutsBackendFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_SHORTCUTS_BACKEND_FACTORY_H_
