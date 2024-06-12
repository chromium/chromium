// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import <memory>
#import <string>

#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "components/translate/core/common/translate_constants.h"
#import "components/translate/core/common/translate_util.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_constants.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/translate/model/translate_app_interface.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_constants.h"
#import "ios/chrome/browser/ui/infobars/infobar_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_translate_modal_constants.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/data_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#import "net/base/url_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForUIElementTimeout;
using chrome_test_util::WebStateScrollViewMatcher;

namespace {

// Infobar requires language detection to happen and may take a little longer
// than kWaitForUIElementTimeout on laggy devices (like test bots).
// Set a longer timeout.
constexpr base::TimeDelta kWaitForUIElement3xTimeout =
    3 * kWaitForUIElementTimeout;

// Paragraph height height for test pages. This must be large enough to trigger
// the fullscreen mode.
const int kParagraphHeightEM = 200;

// Some text in English language.
const char kEnglishText[] =
    "After flying to an altitude of 39,045 meters (128,100 feet) in a "
    "helium-filled balloon, Felix Baumgartner completed a record breaking jump "
    "for the ages from the edge of space, exactly 65 years after Chuck Yeager "
    "first broke the sound barrier flying in an experimental rocket-powered "
    "airplane. Felix reached a maximum of speed of 1,342.8 km/h (833mph) "
    "through the near vacuum of the stratosphere before being slowed by the "
    "atmosphere later during his 4:20 minute long freefall. The 43-year-old "
    "Austrian skydiving expert also broke two other world records (highest "
    "freefall, highest manned balloon flight), leaving the one for the longest "
    "freefall to project mentor Col. Joe Kittinger.";

// Some text in French language.
const char kFrenchText[] =
    "Des yeux qui font baisser les miens. Un rire qui se perd sur sa bouche."
    "Voilà le portrait sans retouches de l'homme auquel j'appartiens "
    "Quand il me prend dans ses bras Il me parle tout bas "
    "Je vois la vie en rose Il me dit des mots d'amour "
    "Des mots de tous les jours Et ça me fait quelque chose "
    "Il est entré dans mon cœur Une part de bonheur Dont je connais la cause "
    "C'est lui pour moi, moi pour lui, dans la vie "
    "Il me l'a dit, l'a juré, pour la vie Et dès que je l'aperçois "
    "Alors je sens en moi, Mon cœur qui bat Des nuits d'amour à plus finir "
    "Un grand bonheur qui prend sa place Les ennuis, les chagrins s'effacent "
    "Heureux, heureux à en mourir Quand il me prend dans ses bras "
    "Il me parle tout bas Je vois la vie en rose Il me dit des mots d'amour "
    "Des mots de tous les jours Et ça me fait quelque chose "
    "Il est entré dans mon cœur Une part de bonheur Dont je connais la cause "
    "C'est toi pour moi, moi pour toi, dans la vie "
    "Tu me l'as dit, l'as juré, pour la vie Et dès que je t'aperçois "
    "Alors je sens en moi Mon cœur qui bat";

// Various HTML tags.
const char kHtmlAttribute[] = "<html>";
const char kHtmlAttributeWithDeLang[] = "<html lang=\"de\">";
const char kMetaNotranslateContent[] =
    "<meta name=\"google\" content=\"notranslate\">";
const char kMetaNotranslateValue[] =
    "<meta name=\"google\" value=\"notranslate\">";
const char kMetaItContentLanguage[] =
    "<meta http-equiv=\"content-language\" content=\"it\">";

// Various link components.
// TODO(crbug.com/41322998): Re-write the hardcoded address.
const char kHttpServerDomain[] = "127.0.0.1";
const char kLanguagePath[] = "/languagepath/";
const char kLinkPath[] = "/linkpath/";
const char kSubresourcePath[] = "/subresourcepath/";
const char kSomeLanguageUrl[] = "http://languagepath/?http=es";
const char kFrenchPagePath[] = "/frenchpage/";
const char kFrenchPageWithLinkPath[] = "/frenchpagewithlink/";
const char kFrenchPageNoTranslateContent[] = "/frenchpagenotranslatecontent/";
const char kFrenchPageNoTranslateValue[] = "/frenchpagenotranslatevalue/";
const char kTranslateScriptPath[] = "/translatescript/";
// Fakes the translate element script by adding a button with the 'Translated'
// label to the web page. The test can check it to determine if the page was
// translated. See translate.js for details.
const char kTranslateScript[] =
    "var google = {"
    "  translate: {"
    "    TranslateService: function(config) {"
    "      return {"
    "        isAvailable: function() {"
    "          return true;"
    "        },"
    "        translatePage: function(source, target, callback) {"
    "          myButton = document.createElement('button');"
    "          myButton.setAttribute('id', 'translated-button');"
    "          myButton.appendChild(document.createTextNode('Translated'));"
    "          document.body.prepend(myButton);"
    "          setTimeout(callback, 1.0, 1.0, true)"
    "        },"
    "        restore: function() {"
    "          myButton = document.getElementById('translated-button');"
    "          myButton.remove();"
    "        }"
    "      }"
    "    }"
    "  }"
    "};"
    "setTimeout(cr.googleTranslate.onTranslateElementLoad, 1.0);";

// Body text for /languagepath/.
const char kLanguagePathText[] = "123456";

// Builds a HTML document with a French text and the given `html` and `meta`
// tags.
std::string GetFrenchPageHtml(const std::string& html_tag,
                              const std::string& meta_tags) {
  return html_tag + meta_tags + "<body>" +
         base::StringPrintf("<p style='height:%dem'>%s</p>", kParagraphHeightEM,
                            kFrenchText) +
         "</body></html>";
}

#pragma mark - TestResponseProvider

// A ResponseProvider that provides html responses of texts in different
// languages or links.
class TestResponseProvider : public web::DataResponseProvider {
 public:
  // TestResponseProvider implementation.
  bool CanHandleRequest(const Request& request) override;
  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override;

 private:
  // Generates a page with a HTTP "Content-Language" header and "httpEquiv" meta
  // tag.
  // The URL in `request` has two parameters, "http" and "meta", that can be
  // used to set the values of the header and the meta tag. For example:
  // http://someurl?http=en&meta=fr generates a page with a "en" HTTP header and
  // a "fr" meta tag.
  void GetLanguageResponse(const Request& request,
                           scoped_refptr<net::HttpResponseHeaders>* headers,
                           std::string* response_body);
};

bool TestResponseProvider::CanHandleRequest(const Request& request) {
  const GURL& url = request.url;
  return (url.host() == kHttpServerDomain &&
          (url.path() == kLanguagePath || url.path() == kLinkPath ||
           url.path() == kSubresourcePath || url.path() == kFrenchPagePath ||
           url.path() == kFrenchPageWithLinkPath ||
           url.path() == kFrenchPageNoTranslateContent ||
           url.path() == kFrenchPageNoTranslateValue ||
           url.path() == kTranslateScriptPath)) ||
         url.SchemeIs(kChromeUIScheme);
}

void TestResponseProvider::GetResponseHeadersAndBody(
    const Request& request,
    scoped_refptr<net::HttpResponseHeaders>* headers,
    std::string* response_body) {
  const GURL& url = request.url;
  *headers = web::ResponseProvider::GetDefaultResponseHeaders();
  if (url.SchemeIs(kChromeUIScheme)) {
    *response_body = url.spec();
    return;
  } else if (url.path() == kLanguagePath) {
    // HTTP header and meta tag read from parameters.
    return GetLanguageResponse(request, headers, response_body);
  } else if (url.path() == kSubresourcePath) {
    // Different "Content-Language" headers in the main page and subresource.
    (*headers)->AddHeader("Content-Language", "fr");
    *response_body = base::StringPrintf(
        "<html><body><img src=%s></body></html>", kSomeLanguageUrl);
    return;
  } else if (url.path() == kLinkPath) {
    // Link to a page with "Content Language" headers.
    GURL some_language_url = web::test::HttpServer::MakeUrl(kSomeLanguageUrl);
    *response_body = base::StringPrintf(
        "<html><body><a href='%s' id='click'>Click</a></body></html>",
        some_language_url.spec().c_str());
    return;
  } else if (url.path() == kFrenchPagePath) {
    *response_body = GetFrenchPageHtml(kHtmlAttribute, "");
    return;
  } else if (url.path() == kFrenchPageWithLinkPath) {
    GURL page_path_url = web::test::HttpServer::MakeUrl(
        base::StringPrintf("http://%s", kFrenchPagePath));
    *response_body = base::StringPrintf(
        "<html><body>%s<br/><a href='%s' id='link'>link</a></body></html>",
        kFrenchText, page_path_url.spec().c_str());
    return;
  } else if (url.path() == kFrenchPageNoTranslateContent) {
    GURL page_path_url = web::test::HttpServer::MakeUrl(
        base::StringPrintf("http://%s", kFrenchPagePath));
    // A page with French text and a 'content' attribute with "notranslate".
    *response_body = GetFrenchPageHtml(kHtmlAttribute, kMetaNotranslateContent);
    return;
  } else if (url.path() == kFrenchPageNoTranslateValue) {
    GURL page_path_url = web::test::HttpServer::MakeUrl(
        base::StringPrintf("http://%s", kFrenchPagePath));
    // A page with French text and a 'value' attribute with "notranslate".
    *response_body = GetFrenchPageHtml(kHtmlAttribute, kMetaNotranslateValue);
    return;
  } else if (url.path() == kTranslateScriptPath) {
    *response_body = kTranslateScript;
    return;
  }
  NOTREACHED_IN_MIGRATION();
}

void TestResponseProvider::GetLanguageResponse(
    const Request& request,
    scoped_refptr<net::HttpResponseHeaders>* headers,
    std::string* response_body) {
  const GURL& url = request.url;
  // HTTP headers.
  std::string http;
  net::GetValueForKeyInQuery(url, "http", &http);
  if (!http.empty())
    (*headers)->AddHeader("Content-Language", http);
  // Response body.
  std::string meta;
  net::GetValueForKeyInQuery(url, "meta", &meta);
  *response_body = "<html>";
  if (!meta.empty()) {
    *response_body += "<head>"
                      "<meta http-equiv='content-language' content='" +
                      meta +
                      "'>"
                      "</head>";
  }
  *response_body +=
      base::StringPrintf("<html><body>%s</body></html>", kLanguagePathText);
}

}  // namespace

#pragma mark - TranslateInfobarTestCase

// Tests for translate.
@interface TranslateInfobarTestCase : WebHttpServerChromeTestCase
@end

@implementation TranslateInfobarTestCase

- (void)setUp {
  [super setUp];

  // Set up the fake URL for the translate script to hit the mock HTTP server.
  GURL translateScriptURL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kTranslateScriptPath));
  NSString* translateScriptSwitchValue =
      base::SysUTF8ToNSString(translateScriptURL.spec());
  [TranslateAppInterface setUpWithScriptServer:translateScriptSwitchValue];
}

- (void)tearDown {
  [TranslateAppInterface tearDown];
  [super tearDown];
}

#pragma mark - Test Cases

// Tests that different language signals are detected correctly.
- (void)testLanguageDetection {
// TODO(crbug.com/40192556): test failing on ipad device
#if !TARGET_IPHONE_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"This test doesn't pass on iPad device.");
  }
#endif
  const GURL URL =
      web::test::HttpServer::MakeUrl("http://scenarioLanguageDetection");
  std::map<GURL, std::string> responses;
  // A page with French text, German "lang" attribute and Italian content
  // language.
  responses[URL] =
      GetFrenchPageHtml(kHtmlAttributeWithDeLang, kMetaItContentLanguage);
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:URL];
  [self assertContentLanguage:@"it"
             htmlRootLanguage:@"de"
              adoptedLanguage:base::SysUTF8ToNSString(
                                  translate::kUnknownLanguageCode)];
}

// Tests that hidden text is not considered during detection.
- (void)testLanguageDetectionIgnoreHiddenText {
  const GURL URL = web::test::HttpServer::MakeUrl(
      "http://scenarioLanguageDetectionIgnoreHiddenText");
  std::map<GURL, std::string> responses;
  // A page with French text that's hidden via CSS.
  responses[URL] = base::StringPrintf(
      "<html><body style='display:none'>%s</body></html>", kFrenchText);
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:URL];
  // Check for no language detected.
  [self assertContentLanguage:@""
             htmlRootLanguage:@""
              adoptedLanguage:base::SysUTF8ToNSString(
                                  translate::kUnknownLanguageCode)];
}

// Tests that language detection is still performed when the page specifies the
// notranslate meta tag.
- (void)testLanguageDetectionNoTranslate {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  const GURL noTranslateContentURL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPageNoTranslateContent));
  const GURL noTranslateValueURL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPageNoTranslateValue));

  // Load some french page with `content="notranslate"| meta tag.
  [ChromeEarlGrey loadURL:noTranslateContentURL];

  // Check that the language has been detected.
  GREYAssertTrue([self waitForLanguageDetection],
                 @"A language has been detected");

  // Load some french page with `value="notranslate"| meta tag.
  [ChromeEarlGrey loadURL:noTranslateValueURL];

  // Check that the language has been detected.
  GREYAssertTrue([self waitForLanguageDetection],
                 @"A language has been detected");
}

// Tests that history.pushState triggers a new detection.
// TODO(crbug.com/40910864): This test is flaky.
- (void)FLAKY_testLanguageDetectionWithPushState {
  const GURL URL = web::test::HttpServer::MakeUrl(
      "http://scenarioLanguageDetectionPushState");
  std::map<GURL, std::string> responses;
  // Page without meaningful text, language should be undefined ("und").
  responses[URL] = "<html><body>Blahrg :)</body></html>";
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:URL];
  // Check for no language detected.
  [self assertContentLanguage:@""
             htmlRootLanguage:@""
              adoptedLanguage:base::SysUTF8ToNSString(
                                  translate::kUnknownLanguageCode)];

  // Resets state before triggering a new round of language detection.
  [TranslateAppInterface resetLanguageDetectionTabHelperObserver];
  // Change the text of the page.
  [ChromeEarlGrey
      evaluateJavaScriptForSideEffect:
          [NSString stringWithFormat:@"document.write('%s');", kEnglishText]];

  // Trigger a new detection with pushState.
  [ChromeEarlGrey
      evaluateJavaScriptForSideEffect:@"history.pushState(null, null, null);"];
  // Check that the new language has been detected.
  [self assertContentLanguage:@"" htmlRootLanguage:@"" adoptedLanguage:@"en"];
}

// Tests that language detection is performed on hash changes.
- (void)testLanguageDetectionWithHashChange {
  // Generate a page with French text and a button that changes the text to
  // English and triggers a hash change.
  std::string html = base::StringPrintf(
      "<html><head><script>"
      "function hash() {"
      "  document.getElementById('text').innerHTML = '%s';"
      "  location.href='#1';"
      "}"
      "</script></head><body>"
      "<input type='button' value='Hash' id='Hash' onclick='hash()'>"
      "<div id='text'>%s</div>"
      "</body></html>",
      kEnglishText, kFrenchText);

  // Set up the mock server.
  std::map<GURL, std::string> responses;
  const GURL URL =
      web::test::HttpServer::MakeUrl("http://hashChangeLanguageDetected");
  responses[URL] = html;
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:URL];
  // Check that language has been detected.
  [self assertContentLanguage:@"" htmlRootLanguage:@"" adoptedLanguage:@"fr"];

  // Resets state before triggering a new round of language detection.
  [TranslateAppInterface resetLanguageDetectionTabHelperObserver];
  // Trigger the hash change.
  [ChromeEarlGrey tapWebStateElementWithID:@"Hash"];
  // Check that language detection has been re-run.
  [self assertContentLanguage:@"" htmlRootLanguage:@"" adoptedLanguage:@"en"];
}

// Tests that language in http content is detected.
- (void)testLanguageDetectionHttpContentLanguage {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // The HTTP header is detected.
  GURL URL = web::test::HttpServer::MakeUrl(std::string("http://") +
                                            kLanguagePath + "?http=fr");
  [ChromeEarlGrey loadURL:URL];
  [self assertContentLanguage:@"fr" htmlRootLanguage:@"" adoptedLanguage:@"fr"];

  // Resets state before triggering a new round of language detection.
  [TranslateAppInterface resetLanguageDetectionTabHelperObserver];
  // Everything after the comma is truncated.
  URL = web::test::HttpServer::MakeUrl(std::string("http://") + kLanguagePath +
                                       "?http=fr,ornot");
  [ChromeEarlGrey loadURL:URL];
  [self assertContentLanguage:@"fr" htmlRootLanguage:@"" adoptedLanguage:@"fr"];

  // Resets state before triggering a new round of language detection.
  [TranslateAppInterface resetLanguageDetectionTabHelperObserver];
  // The HTTP header is overriden by meta tag.
  URL = web::test::HttpServer::MakeUrl(std::string("http://") + kLanguagePath +
                                       "?http=fr&meta=it");
  [ChromeEarlGrey loadURL:URL];
  [self assertContentLanguage:@"it" htmlRootLanguage:@"" adoptedLanguage:@"it"];

  // Resets state before triggering a new round of language detection.
  [TranslateAppInterface resetLanguageDetectionTabHelperObserver];
  // Only the header of the main page is detected.
  URL =
      web::test::HttpServer::MakeUrl(std::string("http://") + kSubresourcePath);
  [ChromeEarlGrey loadURL:URL];
  [self assertContentLanguage:@"fr" htmlRootLanguage:@"" adoptedLanguage:@"fr"];
}

// Tests that language in http content is detected when navigating to a link.
- (void)testLanguageDetectionHttpContentLanguageBehindLink {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Detection works when clicking on a link.
  GURL URL = web::test::HttpServer::MakeUrl(std::string("http://") + kLinkPath);
  GURL someLanguageURL = web::test::HttpServer::MakeUrl(kSomeLanguageUrl);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey tapWebStateElementWithID:@"click"];
  [ChromeEarlGrey waitForWebStateContainingText:kLanguagePathText];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          someLanguageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
  [self assertContentLanguage:@"es" htmlRootLanguage:@"" adoptedLanguage:@"es"];
}

// Tests that language detection still happens when a very large quantity of
// text is present on the page.
- (void)testLanguageDetectionLargePage {
  // Generate very large page.
  std::string html = "<html lang='fr'><body>";
  NSUInteger targetSize = 1024 * 1024;  // More than 1 MB of page content.
  while (html.length() < targetSize) {
    html.append("<p>");
    html.append(kFrenchText);
    html.append("</p>");
  }
  html.append("</body></html>");

  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL URL =
      web::test::HttpServer::MakeUrl("http://languageDetectionLargePage");
  responses[URL] = html;
  web::test::SetUpSimpleHttpServer(responses);
  [ChromeEarlGrey loadURL:URL];

  // Check that language has been detected.
  [self assertContentLanguage:@"" htmlRootLanguage:@"fr" adoptedLanguage:@"fr"];
}

// Tests that language detection is performed but no infobar is triggered when
// translate is disabled.
- (void)testLanguageDetectionDisabled {
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));

  // Disable translate.
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:translate::prefs::kOfferTranslateEnabled];

  // Open some webpage.
  [ChromeEarlGrey loadURL:URL];
  // Wait to be sure language detection has time to happen and benner to appear.
  base::test::ios::SpinRunLoopWithMaxDelay(base::Seconds(2));

  // Check that language has been detected.
  GREYAssert([self waitForLanguageDetection], @"Language not detected");
  // Check Banner was not presented.
  GREYAssertFalse([self isBeforeTranslateBannerVisible],
                  @"Before Translate banner was found");

  // Enable translate.
  [ChromeEarlGrey setBoolValue:YES
                   forUserPref:translate::prefs::kOfferTranslateEnabled];
}

// Tests that the infobar banner persists as the page scrolls mode and that the
// banner can be dimissed.
- (void)testInfobarShowHideDismiss {
  // TODO(crbug.com/334867767): Test fails when run on iOS 17 iPad simulator.
  if (base::ios::IsRunningOnIOS17OrLater() && [ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Fails on iOS 17 iPad simulator.");
  }

  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [ChromeEarlGrey loadURL:URL];

  // Check Banner was presented.
  GREYAssertTrue([self isBeforeTranslateBannerVisible],
                 @"Before Translate banner was not found");

  // Scroll down.
  [[EarlGrey selectElementWithMatcher:WebStateScrollViewMatcher()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  // Expect the translate banner to not be hidden.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Swipe up the Banner.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  // Check Banner was dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Tests that the page can be translated and that translation can be reverted
// using the banner and modal.
- (void)testInfobarTranslateRevert {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [ChromeEarlGrey loadURL:URL];

  // Check Banner was presented.
  GREYAssertTrue([self isBeforeTranslateBannerVisible],
                 @"Before Translate banner was not found");
  // Tap banner button to translate.
  GREYAssertTrue([self selectTranslateButton],
                 @"Could not tap on Translate banner action button");

  // Wait for "Show Original?" banner to appear.
  GREYAssertTrue([self isAfterTranslateBannerVisible],
                 @"Show Original Banner was not found.");

  // Tap on banner button to revert.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                         IDS_IOS_TRANSLATE_INFOBAR_TRANSLATE_UNDO_ACTION)),
                     grey_accessibilityTrait(UIAccessibilityTraitButton), nil)]
      performAction:grey_tap()];

  // Check that badge is not accepted.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBadgeButtonTranslateAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Open modal.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBadgeButtonTranslateAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Translate.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kTranslateInfobarModalTranslateButtonAXId),
                            grey_accessibilityTrait(UIAccessibilityTraitButton),
                            nil)] performAction:grey_tap()];

  // Wait for "Show Original?" banner to appear.
  GREYAssertTrue([self isAfterTranslateBannerVisible],
                 @"Show Original Banner was not found.");
  // Swipe up the Banner.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  // Assert badge is accepted.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBadgeButtonTranslateAcceptedAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
  // Make sure the page is translated.
  [ChromeEarlGrey waitForWebStateContainingText:"Translated"];

  // Open modal from banner.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kBadgeButtonTranslateAcceptedAccessibilityIdentifier),
                     grey_accessibilityTrait(UIAccessibilityTraitButton), nil)]
      performAction:grey_tap()];
  // Revert Translate.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kTranslateInfobarModalShowOriginalButtonAXId),
                            grey_accessibilityTrait(UIAccessibilityTraitButton),
                            nil)] performAction:grey_tap()];

  // Check that badge is not accepted.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBadgeButtonTranslateAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Test that the Show Original banner dismisses with a longer delay since it is
// a high priority banner.
- (void)testInfobarAcceptedBannerDismissWithHighPriorityDelay {
  // TODO(b/338250535): Test is failing on iPad simulator.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Fails on iPad.");
  }
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [ChromeEarlGrey loadURL:URL];

  // Check Banner was presented.
  GREYAssertTrue([self isBeforeTranslateBannerVisible],
                 @"Before Translate banner was not found");
  // Tap banner button to translate.
  GREYAssertTrue([self selectTranslateButton],
                 @"Could not tap on Translate banner action button");

  // Wait for "Show Original?" banner to appear.
  GREYAssertTrue([self isAfterTranslateBannerVisible],
                 @"Show Original Banner was not found.");

  // Wait for banner to dismiss.
  BOOL showOriginalBannerDismiss = WaitUntilConditionOrTimeout(
      kInfobarBannerLongPresentationDuration + base::Seconds(1), ^{
        NSError* error = nil;
        [[EarlGrey
            selectElementWithMatcher:
                grey_allOf(
                    grey_accessibilityID(
                        kInfobarBannerLabelsStackViewIdentifier),
                    grey_descendant(grey_text(l10n_util::GetNSString(
                        IDS_IOS_TRANSLATE_INFOBAR_AFTER_TRANSLATE_BANNER_TITLE))),
                    nil)] assertWithMatcher:grey_nil() error:&error];
        return error == nil;
      });
  GREYAssertTrue(showOriginalBannerDismiss,
                 @"Show Original Banner did not dismiss");
  // Check that badge is accepted.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBadgeButtonTranslateAcceptedAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Tests that the page can be translated and that translation can be reverted
// in incognito mode.
- (void)testInfobarTranslateRevertIncognito {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [TranslateAppInterface tearDownLanguageDetectionTabHelperObserver];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:URL];

  // Check Banner was presented.
  GREYAssertTrue([self isBeforeTranslateBannerVisible],
                 @"Before Translate banner was not found");
  // Tap banner button to translate.
  GREYAssertTrue([self selectTranslateButton],
                 @"Could not tap on Translate banner action button");

  // Wait for "Show Original?" banner to appear.
  GREYAssertTrue([self isAfterTranslateBannerVisible],
                 @"Show Original Banner was not found.");
  // Make sure the page was translated.
  [ChromeEarlGrey waitForWebStateContainingText:"Translated"];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBadgeButtonTranslateAcceptedAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Tap on banner button to revert.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                         IDS_IOS_TRANSLATE_INFOBAR_TRANSLATE_UNDO_ACTION)),
                     grey_accessibilityTrait(UIAccessibilityTraitButton), nil)]
      performAction:grey_tap()];

  // Check that badge is not accepted.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBadgeButtonTranslateAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Tests that the target language can be changed. TODO(crbug.com/40670920):
// implement test for changing source language.
- (void)testInfobarChangeTargetLanguage {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text and a link.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPageWithLinkPath));
  [ChromeEarlGrey loadURL:URL];

  // Check Translate banner is presented.
  GREYAssertTrue([self isBeforeTranslateBannerVisible],
                 @"Before Translate banner was not found");

  // Show modal.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(
                                       kInfobarBannerOpenModalButtonIdentifier),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];

  // Select Target row.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kTranslateInfobarModalTranslateTargetLanguageItemAXId)]
      performAction:grey_tap()];
  // Select "Dutch" from the table view.
  [[[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Dutch")]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 300)
      onElementWithMatcher:grey_accessibilityID(
                               kTranslateInfobarLanguageSelectionTableViewAXId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"To, Dutch")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kTranslateInfobarModalTranslateButtonAXId),
                            grey_accessibilityTrait(UIAccessibilityTraitButton),
                            nil)] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:"Translated"];

  GREYAssertTrue([self isAfterTranslateBannerVisible],
                 @"Show Original Banner was not found.");
}

// Tests that the "Always Translate" options can be toggled and the prefs are
// updated accordingly.
// TODO(crbug.com/334867767) Fix and reenable tests.
- (void)testInfobarAlwaysTranslate {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [ChromeEarlGrey loadURL:URL];

  // Make sure that French to English translation is not automatic.
  GREYAssert(![TranslateAppInterface shouldAutoTranslateFromLanguage:@"fr"
                                                          toLanguage:@"en"],
             @"French to English translation is automatic");

  // Check Translate banner is presented.
  GREYAssertTrue([self isBeforeTranslateBannerVisible],
                 @"Before Translate banner was not found");
  // Show modal.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(
                                       kInfobarBannerOpenModalButtonIdentifier),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];
  // Select the Always Translate button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kTranslateInfobarModalAlwaysTranslateButtonAXId),
                     grey_accessibilityTrait(UIAccessibilityTraitButton), nil)]
      performAction:grey_tap()];

  // Make sure the page is translated.
  [ChromeEarlGrey waitForWebStateContainingText:"Translated"];
  GREYAssertTrue([self isAfterTranslateBannerVisible],
                 @"Show Original Banner was not found.");
  // Swipe up the Banner.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  // Make sure that French to English translation is automatic.
  GREYAssert([TranslateAppInterface shouldAutoTranslateFromLanguage:@"fr"
                                                         toLanguage:@"en"],
             @"French to English translation is not automatic");

  // Reload the page.
  [ChromeEarlGrey reload];

  // Check that badge is accepted.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBadgeButtonTranslateAcceptedAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
  // Make sure the page is translated.
  [ChromeEarlGrey waitForWebStateContainingText:"Translated"];
}

// Tests that the "Never Translate ..." options dismisses the infobar and
// updates the prefs accordingly.
- (void)testInfobarNeverTranslate {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [ChromeEarlGrey loadURL:URL];

  // Make sure that French to English translation is not automatic.
  GREYAssert(![TranslateAppInterface shouldAutoTranslateFromLanguage:@"fr"
                                                          toLanguage:@"en"],
             @"French to English translation is automatic");

  // Check Translate banner is presented.
  GREYAssertTrue([self isBeforeTranslateBannerVisible],
                 @"Before Translate banner was not found");
  // Show modal.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(
                                       kInfobarBannerOpenModalButtonIdentifier),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];
  // Select the Never Translate button.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kTranslateInfobarModalNeverTranslateButtonAXId),
                            grey_accessibilityTrait(UIAccessibilityTraitButton),
                            nil)] performAction:grey_tap()];

  // Check that there is no badge.
  [[EarlGrey
      selectElementWithMatcher:
          grey_anyOf(grey_accessibilityID(
                         kBadgeButtonTranslateAccessibilityIdentifier),
                     grey_accessibilityID(
                         kBadgeButtonTranslateAcceptedAccessibilityIdentifier),
                     nil)] assertWithMatcher:grey_nil()];

  // Make sure that translation from French is blocked.
  GREYAssert([TranslateAppInterface isBlockedLanguage:@"fr"],
             @"Translation from French is not blocked");

  // Reload the page.
  [ChromeEarlGrey reload];

  // Check that there is no badge.
  [[EarlGrey
      selectElementWithMatcher:
          grey_anyOf(grey_accessibilityID(
                         kBadgeButtonTranslateAccessibilityIdentifier),
                     grey_accessibilityID(
                         kBadgeButtonTranslateAcceptedAccessibilityIdentifier),
                     nil)] assertWithMatcher:grey_nil()];
}

// Tests that the "Never Translate this site" option dismisses the infobar and
// updates the prefs accordingly.
- (void)testInfobarNeverTranslateSite {
  // TODO(crbug.com/334867767): Test fails when run on iOS 17 iPad simulator.
  if (base::ios::IsRunningOnIOS17OrLater() && [ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Fails on iOS 17 iPad simulator.");
  }

  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [ChromeEarlGrey loadURL:URL];

  NSString* URLHost = base::SysUTF8ToNSString(URL.HostNoBrackets());
  // Make sure that translation for the site is not blocked.
  GREYAssert(![TranslateAppInterface isBlockedSite:URLHost],
             @"Translation is blocked for the site");

  // Check Translate banner is presented.
  GREYAssertTrue([self isBeforeTranslateBannerVisible],
                 @"Before Translate banner was not found");
  // Show modal.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(
                                       kInfobarBannerOpenModalButtonIdentifier),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];
  // Select the Never Translate Site button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kTranslateInfobarModalNeverTranslateSiteButtonAXId),
                     grey_accessibilityTrait(UIAccessibilityTraitButton), nil)]
      performAction:grey_tap()];

  // Check that there is no badge.
  [[EarlGrey
      selectElementWithMatcher:
          grey_anyOf(grey_accessibilityID(
                         kBadgeButtonTranslateAccessibilityIdentifier),
                     grey_accessibilityID(
                         kBadgeButtonTranslateAcceptedAccessibilityIdentifier),
                     nil)] assertWithMatcher:grey_nil()];
  // Make sure that translation for the site is blocked.
  GREYAssert([TranslateAppInterface isBlockedSite:URLHost],
             @"Translation is not blocked for the site");

  // Reload the page.
  [ChromeEarlGrey reload];

  // Check that there is no badge.
  [[EarlGrey
      selectElementWithMatcher:
          grey_anyOf(grey_accessibilityID(
                         kBadgeButtonTranslateAccessibilityIdentifier),
                     grey_accessibilityID(
                         kBadgeButtonTranslateAcceptedAccessibilityIdentifier),
                     nil)] assertWithMatcher:grey_nil()];
}

// Tests that the "Translate..." button in the tools menu is enabled if
// translate is available and it brings up the Translate infobar and translates
// the page when tapped.
- (void)testTranslateManualTrigger {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [ChromeEarlGrey loadURL:URL];

  // Check Translate banner is presented.
  GREYAssertTrue([self isBeforeTranslateBannerVisible],
                 @"Before Translate banner was not found");
  // Swipe up the Banner.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  // Make sure the Translate manual trigger button is enabled and tap it.
  [ChromeEarlGreyUI openToolsMenu];
  [[[[EarlGrey selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                       kToolsMenuTranslateId),
                                                   grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 250)
      onElementWithMatcher:chrome_test_util::ToolsMenuView()]
      assertWithMatcher:grey_not(grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled))]
      performAction:grey_tap()];

  // Wait for "Show Original?" banner to appear.
  GREYAssertTrue([self isAfterTranslateBannerVisible],
                 @"Show Original Banner was not found.");
}

// Test that tapping cancel in the Modal doesn't save changes to source/target
// languages and doesn't start a Translate
- (void)testTranslateModalCancel {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Load a page with French text.
  GURL URL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [ChromeEarlGrey loadURL:URL];

  // Check Translate banner is presented.
  GREYAssertTrue([self isBeforeTranslateBannerVisible],
                 @"Before Translate banner was not found");
  // Show modal.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(
                                       kInfobarBannerOpenModalButtonIdentifier),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];

  // Select Target row.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kTranslateInfobarModalTranslateTargetLanguageItemAXId)]
      performAction:grey_tap()];
  // Select "Dutch" from the table view.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(@"Dutch"),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 300)
      onElementWithMatcher:grey_accessibilityID(
                               kTranslateInfobarLanguageSelectionTableViewAXId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"To, Dutch")]
      assertWithMatcher:grey_notNil()];

  // Select Source row.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kTranslateInfobarModalTranslateSourceLanguageItemAXId)]
      performAction:grey_tap()];
  // Select "Dutch" from the table view.
  [[[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"English")]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 300)
      onElementWithMatcher:grey_accessibilityID(
                               kTranslateInfobarLanguageSelectionTableViewAXId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"From, English")]
      assertWithMatcher:grey_notNil()];

  // Exit out of modal by tapping on Cancel
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kInfobarModalCancelButton),
                                          grey_accessibilityTrait(
                                              UIAccessibilityTraitButton),
                                          nil)] performAction:grey_tap()];

  // Open modal by selecting the badge that shouldn't be accepted.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBadgeButtonTranslateAccessibilityIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"From, French")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"To, English")]
      assertWithMatcher:grey_notNil()];
}

#pragma mark - Utility methods

- (BOOL)isBeforeTranslateBannerVisible {
  BOOL bannerShown = WaitUntilConditionOrTimeout(kWaitForUIElement3xTimeout, ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:
            grey_allOf(
                grey_accessibilityID(kInfobarBannerLabelsStackViewIdentifier),
                grey_descendant(grey_text(l10n_util::GetNSString(
                    IDS_IOS_TRANSLATE_INFOBAR_BEFORE_TRANSLATE_BANNER_TITLE))),
                nil)] assertWithMatcher:grey_notNil() error:&error];
    return error == nil;
  });

  return bannerShown;
}

- (BOOL)isAfterTranslateBannerVisible {
  BOOL showOriginalBannerShown =
      WaitUntilConditionOrTimeout(kWaitForUIElement3xTimeout, ^{
        NSError* error = nil;
        [[EarlGrey
            selectElementWithMatcher:
                grey_allOf(
                    grey_accessibilityID(
                        kInfobarBannerLabelsStackViewIdentifier),
                    grey_descendant(grey_text(l10n_util::GetNSString(
                        IDS_IOS_TRANSLATE_INFOBAR_AFTER_TRANSLATE_BANNER_TITLE))),
                    nil)] assertWithMatcher:grey_notNil() error:&error];
        return error == nil;
      });
  return showOriginalBannerShown;
}

- (BOOL)selectTranslateButton {
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                                IDS_IOS_TRANSLATE_INFOBAR_TRANSLATE_ACTION)),
                            grey_accessibilityTrait(UIAccessibilityTraitButton),
                            nil)] performAction:grey_tap() error:&error];
  return error == nil;
}

// Returns whether a language has been detected on the current page. Returns
// false if a timeout was detected while waiting for language detection.
- (BOOL)waitForLanguageDetection {
  bool detected = WaitUntilConditionOrTimeout(kWaitForUIElement3xTimeout, ^{
    return [TranslateAppInterface isLanguageDetected];
  });
  return detected;
}

// Waits until language details have been detected then verifies them.
// Checks expectation for Content-Language, HTML root element language, and
// detected language on page. Use @"" for expected values that are expected
// to be not set.
- (void)assertContentLanguage:(NSString*)expectedContentLanguage
             htmlRootLanguage:(NSString*)expectedHtmlRootLanguage
              adoptedLanguage:(NSString*)expectedAdoptedLanguage {
  GREYAssert([self waitForLanguageDetection], @"Language not detected");

  NSString* contentLanguage = [TranslateAppInterface contentLanguage];
  NSString* contentLanguageError =
      [NSString stringWithFormat:@"Wrong content-language: %@ (expected %@)",
                                 contentLanguage, expectedContentLanguage];
  GREYAssertEqualObjects(expectedContentLanguage, contentLanguage,
                         contentLanguageError);

  NSString* htmlRootLanguage = [TranslateAppInterface htmlRootLanguage];
  NSString* htmlRootLanguageError =
      [NSString stringWithFormat:@"Wrong html root language: %@ (expected %@)",
                                 htmlRootLanguage, expectedHtmlRootLanguage];
  GREYAssertEqualObjects(expectedHtmlRootLanguage, htmlRootLanguage,
                         htmlRootLanguageError);

  NSString* adoptedLanguage = [TranslateAppInterface adoptedLanguage];
  NSString* adoptedLanguageError =
      [NSString stringWithFormat:@"Wrong adopted language: %@ (expected %@)",
                                 adoptedLanguage, expectedAdoptedLanguage];
  GREYAssertEqualObjects(expectedAdoptedLanguage, adoptedLanguage,
                         adoptedLanguageError);
}

@end
