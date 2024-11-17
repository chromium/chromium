// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp_tiles/model/ios_popular_sites_factory.h"

#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "components/ntp_tiles/popular_sites_impl.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/thread/web_thread.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

std::unique_ptr<ntp_tiles::PopularSites>
IOSPopularSitesFactory::NewForBrowserState(ProfileIOS* profile) {
  return std::make_unique<ntp_tiles::PopularSitesImpl>(
      profile->GetPrefs(),
      ios::TemplateURLServiceFactory::GetForProfile(profile),
      GetApplicationContext()->GetVariationsService(),
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          profile->GetURLLoaderFactory()));
}
