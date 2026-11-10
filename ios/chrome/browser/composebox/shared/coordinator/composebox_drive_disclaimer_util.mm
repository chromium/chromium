// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/shared/coordinator/composebox_drive_disclaimer_util.h"

#import "base/check.h"
#import "components/contextual_search/footprints/public/drive_disclaimer_controller.h"
#import "components/contextual_search/footprints/public/fpop_service.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace composebox {

std::unique_ptr<drive_picker::DriveDisclaimerController>
CreateDriveDisclaimerController(Browser* browser) {
  CHECK(browser);
  ProfileIOS* profile = browser->GetProfile();
  if (profile->IsOffTheRecord()) {
    return nullptr;
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      profile->GetSharedURLLoaderFactory();
  auto fpop_service = contextual_search::FpopService::Create(
      identity_manager, url_loader_factory);
  return std::make_unique<drive_picker::DriveDisclaimerController>(
      std::move(fpop_service));
}

}  // namespace composebox
