// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_app_interface.h"

#import "base/files/file_util.h"
#import "base/no_destructor.h"
#import "base/path_service.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/google/core/common/google_util.h"
#import "components/history/core/browser/top_sites.h"
#import "components/omnibox/browser/shortcuts_backend.h"
#import "components/search_engines/template_url_service.h"
#import "components/variations/variations_ids_provider.h"
#import "ios/chrome/browser/autocomplete/model/remote_suggestions_service_factory.h"
#import "ios/chrome/browser/autocomplete/model/shortcuts_backend_factory.h"
#import "ios/chrome/browser/history/top_sites_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/omnibox/test_fake_suggestions_service.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

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

// Returns the directory containing fake suggestions files.
const base::FilePath& GetTestDataDir() {
  static base::NoDestructor<base::FilePath> dir([]() {
    base::FilePath dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &dir);
    return dir.AppendASCII("ios")
        .AppendASCII("chrome")
        .AppendASCII("test")
        .AppendASCII("data")
        .AppendASCII("omnibox");
  }());
  return *dir;
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

+ (void)setUpFakeSuggestionsService:(NSString*)filename {
  RemoteSuggestionsService* remoteSuggestionsService =
      RemoteSuggestionsServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState(), YES);

  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  base::FilePath filePath =
      GetTestDataDir().AppendASCII(base::SysNSStringToUTF8(filename));
  TestFakeSuggestionsService::GetInstance()->SetUp(
      remoteSuggestionsService, templateURLService, filePath);
}

+ (void)tearDownFakeSuggestionsService {
  RemoteSuggestionsService* remoteSuggestionsService =
      RemoteSuggestionsServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState(), YES);

  network::mojom::URLLoaderFactory* urlLoaderFactory =
      chrome_test_util::GetOriginalBrowserState()->GetURLLoaderFactory();

  TestFakeSuggestionsService::GetInstance()->TearDown(remoteSuggestionsService,
                                                      urlLoaderFactory);
}

+ (BOOL)shortcutsBackendInitialized {
  scoped_refptr<ShortcutsBackend> shortcuts_backend =
      ios::ShortcutsBackendFactory::GetInstance()->GetForBrowserStateIfExists(
          chrome_test_util::GetOriginalBrowserState());
  if (shortcuts_backend) {
    return shortcuts_backend->initialized();
  }
  return NO;
}

+ (NSInteger)numberOfShortcutsInDatabase {
  scoped_refptr<ShortcutsBackend> shortcuts_backend =
      ios::ShortcutsBackendFactory::GetInstance()->GetForBrowserStateIfExists(
          chrome_test_util::GetOriginalBrowserState());
  if (shortcuts_backend && shortcuts_backend->initialized()) {
    return static_cast<NSInteger>(shortcuts_backend->shortcuts_map().size());
  }
  return 0;
}

@end
