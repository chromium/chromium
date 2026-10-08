// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_SHORTCUTS_BACKEND_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_SHORTCUTS_BACKEND_FACTORY_H_

#import "components/omnibox/browser/shortcuts_backend.h"
#import "ios/chrome/browser/shared/model/profile/typed_refcounted_profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace ios {

// Singleton that owns all ShortcutsBackends and associates them with
// ProfileIOS.
class ShortcutsBackendFactory
    : public TypedRefcountedProfileKeyedServiceFactoryIOS<
          ShortcutsBackendFactory,
          ShortcutsBackend> {
 public:
  ShortcutsBackendFactory(PassKey key);

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

 private:
  // RefcountedProfileKeyedServiceFactoryIOS implementation.
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_SHORTCUTS_BACKEND_FACTORY_H_
