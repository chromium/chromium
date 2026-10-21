// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_LOG_ROUTER_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_LOG_ROUTER_FACTORY_H_

#import "components/autofill/core/browser/logging/log_router.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace autofill {

// A factory that associates LogRouter instances with profiles. This returns
// nullptr for off-the-record profiles.
class AutofillLogRouterFactory final
    : public TypedProfileKeyedServiceFactoryIOS<AutofillLogRouterFactory,
                                                LogRouter> {
 public:
  explicit AutofillLogRouterFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace autofill
#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_LOG_ROUTER_FACTORY_H_
