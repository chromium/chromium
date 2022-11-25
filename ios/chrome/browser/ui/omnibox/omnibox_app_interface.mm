// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_app_interface.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/google/core/common/google_util.h"
#import "components/history/core/browser/top_sites.h"
#import "components/variations/variations_ids_provider.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/history/top_sites_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Rewrite google URLs to localhost so they can be loaded by the test server.
bool GoogleToLocalhostURLRewriter(GURL* url, web::BrowserState* browser_state) {
  if (!google_util::IsGoogleDomainUrl(*url, google_util::DISALLOW_SUBDOMAIN,
                                      google_util::ALLOW_NON_STANDARD_PORTS))
    return false;
  GURL rewritten_url(*url);
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpScheme);
  replacements.SetHostStr("127.0.0.1");

  rewritten_url = rewritten_url.ReplaceComponents(replacements);
  *url = rewritten_url;

  return true;
}

}  // namespace

@implementation OmniboxAppInterface

+ (void)rewriteGoogleURLToLocalhost {
  chrome_test_util::GetCurrentWebState()
      ->GetNavigationManager()
      ->AddTransientURLRewriter(&GoogleToLocalhostURLRewriter);
}

+ (BOOL)forceVariationID:(int)variationID {
  return variations::VariationsIdsProvider::ForceIdsResult::SUCCESS ==
         variations::VariationsIdsProvider::GetInstance()->ForceVariationIds(
             /*variation_ids=*/{base::NumberToString(variationID)},
             /*command_line_variation_ids=*/"");
}

+ (void)blockURLFromTopSites:(NSString*)URL {
  scoped_refptr<history::TopSites> top_sites =
      ios::TopSitesFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  if (top_sites) {
    top_sites->AddBlockedUrl(GURL(base::SysNSStringToUTF8(URL)));
  }
}

@end
