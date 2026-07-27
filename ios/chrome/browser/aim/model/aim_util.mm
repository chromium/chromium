// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/model/aim_util.h"

#import "base/check.h"
#import "base/time/time.h"
#import "components/lens/lens_overlay_invocation_source.h"
#import "components/search_engines/util.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#import "url/gurl.h"

void OpenAimInCurrentTab(TemplateURLService* template_url_service,
                         UrlLoadingBrowserAgent* url_loader,
                         omnibox::ChromeAimEntryPoint entry_point,
                         lens::LensOverlayInvocationSource invocation_source,
                         BOOL in_incognito) {
  CHECK(url_loader);
  GURL aim_url =
      GetUrlForAim(template_url_service, entry_point, base::Time::Now(),
                   /*query_text=*/u"", invocation_source,
                   /*additional_params=*/{});
  url_loader->Load(UrlLoadParams::InCurrentTab(aim_url));
}

void OpenAppBarAimInCurrentTab(TemplateURLService* template_url_service,
                               UrlLoadingBrowserAgent* url_loader,
                               BOOL in_incognito) {
  OpenAimInCurrentTab(
      template_url_service, url_loader,
      omnibox::ChromeAimEntryPoint::IOS_CHROME_APP_BAR_ENTRY_POINT,
      lens::LensOverlayInvocationSource::kAppBarAimButton, in_incognito);
}
