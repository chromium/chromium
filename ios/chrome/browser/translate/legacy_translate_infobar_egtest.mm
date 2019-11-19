// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_switches.h"
#include "components/translate/ios/browser/ios_translate_driver.h"
#import "components/translate/ios/browser/js_translate_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#include "ios/chrome/browser/ui/translate/language_selection_view_controller.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/earl_grey/js_test_util.h"
#include "ios/web/public/test/http_server/data_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

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

// Various link components.
// TODO(crbug.com/729195): Re-write the hardcoded address.
const char kHttpServerDomain[] = "127.0.0.1";
const char kLanguagePath[] = "/languagepath/";
const char kLinkPath[] = "/linkpath/";
const char kSubresourcePath[] = "/subresourcepath/";
const char kSomeLanguageUrl[] = "http://languagepath/?http=es";
const char kFrenchPagePath[] = "/frenchpage/";
const char kFrenchPageWithLinkPath[] = "/frenchpagewithlink/";
const char kTranslateScriptPath[] = "/translatescript/";
const char kTranslateScript[] = "Fake Translate Script";

// Body text for /languagepath/.
const char kLanguagePathText[] = "Some text here.";

// Builds a HTML document with a French text and the given |html| and |meta|
// tags.
std::string GetFrenchPageHtml(const std::string& html_tag,
                              const std::string& meta_tags) {
  return html_tag + meta_tags + "<body>" + kFrenchText + "</body></html>";
}

// Returns the label of the "Always translate" switch in the Translate infobar.
NSString* GetTranslateInfobarSwitchLabel(const std::string& language) {
  return base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
      IDS_TRANSLATE_INFOBAR_ALWAYS_TRANSLATE, base::UTF8ToUTF16(language)));
}

// Returns a matcher for the button with label "Cancel" in the language picker.
// The language picker uses the system accessibility labels, thus no IDS_CANCEL.
id<GREYMatcher> LanguagePickerCancelButton() {
  return grey_accessibilityID(kLanguagePickerCancelButtonId);
}

// Returns a matcher for the button with label "Done" in the language picker.
// The language picker uses the system accessibility labels, thus no IDS_DONE.
id<GREYMatcher> LanguagePickerDoneButton() {
  return grey_accessibilityID(kLanguagePickerDoneButtonId);
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
  // The URL in |request| has two parameters, "http" and "meta", that can be
  // used to set the values of the header and the meta tag. For example:
  // http://someurl?http=en&meta=fr generates a page with a "en" HTTP header and
  // a "fr" meta tag.
  void GetLanguageResponse(const Request& request,
                           scoped_refptr<net::HttpResponseHeaders>* headers,
                           std::string* response_body);
};

bool TestResponseProvider::CanHandleRequest(const Request& request) {
  const GURL& url = request.url;
  return url.host() == kHttpServerDomain &&
         (url.path() == kLanguagePath || url.path() == kLinkPath ||
          url.path() == kSubresourcePath || url.path() == kFrenchPagePath ||
          url.path() == kFrenchPageWithLinkPath ||
          url.path() == kTranslateScriptPath);
}

void TestResponseProvider::GetResponseHeadersAndBody(
    const Request& request,
    scoped_refptr<net::HttpResponseHeaders>* headers,
    std::string* response_body) {
  const GURL& url = request.url;
  *headers = web::ResponseProvider::GetDefaultResponseHeaders();
  if (url.path() == kLanguagePath) {
    // HTTP header and meta tag read from parameters.
    return GetLanguageResponse(request, headers, response_body);
  } else if (url.path() == kSubresourcePath) {
    // Different "Content-Language" headers in the main page and subresource.
    (*headers)->AddHeader("Content-Language: fr");
    *response_body = base::StringPrintf(
        "<html><body><img src=%s></body></html>", kSomeLanguageUrl);
    return;
  } else if (url.path() == kLinkPath) {
    // Link to a page with "Content Language" headers.
    GURL url = web::test::HttpServer::MakeUrl(kSomeLanguageUrl);
    *response_body = base::StringPrintf(
        "<html><body><a href='%s' id='click'>Click</a></body></html>",
        url.spec().c_str());
    return;
  } else if (url.path() == kFrenchPagePath) {
    *response_body =
        base::StringPrintf("<html><body>%s</body></html>", kFrenchText);
    return;
  } else if (url.path() == kFrenchPageWithLinkPath) {
    GURL page_path_url = web::test::HttpServer::MakeUrl(
        base::StringPrintf("http://%s", kFrenchPagePath));
    *response_body = base::StringPrintf(
        "<html><body>%s<br /><a href='%s' id='link'>link</a></body></html>",
        kFrenchText, page_path_url.spec().c_str());
    return;
  } else if (url.path() == kTranslateScriptPath) {
    *response_body = kTranslateScript;
    return;
  }
  NOTREACHED();
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
    (*headers)->AddHeader(std::string("Content-Language: ") + http);
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

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::CloseButton;
using translate::LanguageDetectionController;

#pragma mark - MockTranslateScriptManager

// Mock javascript translate manager that does not use the translate servers.
// Translating the page just adds a 'Translated' button to the page, without
// changing the text.
@interface MockTranslateScriptManager : JsTranslateManager {
  web::WebState* _webState;  // weak
}

- (instancetype)initWithWebState:(web::WebState*)webState;

@end

@implementation MockTranslateScriptManager

- (instancetype)initWithWebState:(web::WebState*)webState {
  if ((self = [super init])) {
    _webState = webState;
  }
  return self;
}

- (void)setScript:(NSString*)script {
}

- (void)startTranslationFrom:(const std::string&)source
                          to:(const std::string&)target {
  // Add a button with the 'Translated' label to the web page.
  // The test can check it to determine if this method has been called.
  _webState->ExecuteJavaScript(base::UTF8ToUTF16(
      "myButton = document.createElement('button');"
      "myButton.appendChild(document.createTextNode('Translated'));"
      "document.body.appendChild(myButton);"));
}

- (void)inject {
  // Prevent the actual script from being injected and instead just invoke host
  // with 'translate.ready' followed by 'translate.status'.
  _webState->ExecuteJavaScript(
      base::UTF8ToUTF16("__gCrWeb.message.invokeOnHost({"
                        "  'command': 'translate.ready',"
                        "  'errorCode': 0,"
                        "  'loadTime': 0,"
                        "  'readyTime': 0});"));
  _webState->ExecuteJavaScript(
      base::UTF8ToUTF16("__gCrWeb.message.invokeOnHost({"
                        "  'command': 'translate.status',"
                        "  'errorCode': 0,"
                        "  'originalPageLanguage': 'fr',"
                        "  'translationTime': 0});"));
}

@end

#pragma mark - TranslateTestCase

// Tests for translate.
@interface TranslateTestCase : ChromeTestCase

@end

@implementation TranslateTestCase

+ (void)setUp {
  [super setUp];
  if ([ChromeEarlGrey isCompactTranslateInfobarIOSEnabled]) {
    // translate::kCompactTranslateInfobarIOS feature is enabled. You need
    // to pass --disable-features=CompactTranslateInfobarIOS command line
    // argument in order to run this test.
    DCHECK(false);
  }
}

- (void)setUp {
  [super setUp];
  // Reset translate prefs to default.
  std::unique_ptr<translate::TranslatePrefs> translatePrefs(
      ChromeIOSTranslateClient::CreateTranslatePrefs(
          chrome_test_util::GetOriginalBrowserState()->GetPrefs()));
  translatePrefs->ResetToDefaults();
}

- (void)tearDown {
  // Reset translate prefs to default.
  std::unique_ptr<translate::TranslatePrefs> translatePrefs(
      ChromeIOSTranslateClient::CreateTranslatePrefs(
          chrome_test_util::GetOriginalBrowserState()->GetPrefs()));
  translatePrefs->ResetToDefaults();

  // Do not allow offering translate in builds without an API key.
  translate::TranslateManager::SetIgnoreMissingKeyForTesting(false);
  [super tearDown];
}

#pragma mark - Test Cases

// Tests that the language detection infobar is displayed.
- (void)testLanguageDetectionInfobar {
  const GURL URL =
      web::test::HttpServer::MakeUrl("http://scenarioLanguageDetectionInfobar");
  std::map<GURL, std::string> responses;
  // A page with French text.
  responses[URL] = GetFrenchPageHtml(kHtmlAttribute, "");
  web::test::SetUpSimpleHttpServer(responses);
  [ChromeEarlGrey loadURL:URL];

  // Check that the "Before Translate" infobar is displayed.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"English")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_TRANSLATE_INFOBAR_ACCEPT)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:CloseButton()]
      assertWithMatcher:grey_notNil()];

  // Open the language picker.
  NSString* kFrench = @"French";
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(kFrench)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:LanguagePickerCancelButton()]
      assertWithMatcher:grey_notNil()];

  // Change the language using the picker.
  NSString* const kPickedLanguage = @"Finnish";
  id<GREYMatcher> languageMatcher = grey_allOf(
      chrome_test_util::StaticTextWithAccessibilityLabel(kPickedLanguage),
      grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:languageMatcher]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:LanguagePickerDoneButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(kPickedLanguage)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(kFrench)]
      assertWithMatcher:grey_nil()];

  // Deny the translation, and check that the infobar is dismissed.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_TRANSLATE_INFOBAR_DENY)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(kPickedLanguage)]
      assertWithMatcher:grey_nil()];
}

// Tests that the Translate infobar is displayed after translation.
- (void)testTranslateInfobar {
  const GURL URL =
      web::test::HttpServer::MakeUrl("http://scenarioTranslateInfobar");
  std::map<GURL, std::string> responses;
  // A page with some text.
  responses[URL] = "<html><body>Hello world!</body></html>";
  web::test::SetUpSimpleHttpServer(responses);

  // Assert that Spanish to English translation is disabled.
  std::unique_ptr<translate::TranslatePrefs> translatePrefs(
      ChromeIOSTranslateClient::CreateTranslatePrefs(
          chrome_test_util::GetOriginalBrowserState()->GetPrefs()));
  GREYAssert(!translatePrefs->IsLanguagePairWhitelisted("es", "en"),
             @"Translate Spanish is enabled");
  // Increase accepted translation count for Spanish
  for (int i = 0; i < 3; i++) {
    translatePrefs->IncrementTranslationAcceptedCount("es");
  }

  // Open a new webpage.
  [ChromeEarlGrey loadURL:URL];
  [self simulateTranslationFromSpanishToEnglish];

  // Check that the "Always Translate" switch is displayed in the infobar.
  NSString* switchLabel = GetTranslateInfobarSwitchLabel("Spanish");
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(switchLabel)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Toggle "Always Translate" and check the preference.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(switchLabel)]
      performAction:grey_tap()];
  id<GREYMatcher> switchOn =
      grey_allOf(ButtonWithAccessibilityLabel(switchLabel),
                 grey_accessibilityValue(@"1"), nil);
  [[EarlGrey selectElementWithMatcher:switchOn]
      assertWithMatcher:grey_notNil()];

  // Assert that Spanish to English translation is not enabled after tapping
  // the switch (should only be saved when "Done" button is tapped).
  GREYAssert(!translatePrefs->IsLanguagePairWhitelisted("es", "en"),
             @"Translate Spanish is disabled");

  // Tap the "Done" button to save the preference.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"Done")]
      performAction:grey_tap()];

  // Assert that Spanish to English translation is enabled.
  GREYAssert(translatePrefs->IsLanguagePairWhitelisted("es", "en"),
             @"Translate Spanish is disabled");
}

// Tests that the "Always Translate" switch is not shown in incognito mode.
- (void)testIncognitoTranslateInfobar {
  const GURL URL =
      web::test::HttpServer::MakeUrl("http://scenarioTranslateInfobar");
  std::map<GURL, std::string> responses;
  // A page with some text.
  responses[URL] = "<html><body>Hello world!</body></html>";
  web::test::SetUpSimpleHttpServer(responses);

  // Increased accepted translation count for Spanish.
  std::unique_ptr<translate::TranslatePrefs> translatePrefs(
      ChromeIOSTranslateClient::CreateTranslatePrefs(
          chrome_test_util::GetOriginalBrowserState()->GetPrefs()));
  for (int i = 0; i < 3; i++) {
    translatePrefs->IncrementTranslationAcceptedCount("es");
  }

  // Do a translation in incognito
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:URL];
  [self simulateTranslationFromSpanishToEnglish];
  // Check that the infobar does not contain the "Always Translate" switch.
  NSString* switchLabel = GetTranslateInfobarSwitchLabel("Spanish");
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(switchLabel)]
      assertWithMatcher:grey_nil()];
}

// Tests that translation occurs automatically on second navigation to an
// already translated page.
- (void)testAutoTranslateInfobar {
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));

  // Set up the mock translate script manager.
  ChromeIOSTranslateClient* client = ChromeIOSTranslateClient::FromWebState(
      chrome_test_util::GetCurrentWebState());
  translate::IOSTranslateDriver* driver =
      static_cast<translate::IOSTranslateDriver*>(client->GetTranslateDriver());
  MockTranslateScriptManager* jsTranslateManager =
      [[MockTranslateScriptManager alloc]
          initWithWebState:chrome_test_util::GetCurrentWebState()];
  driver->translate_controller()->SetJsTranslateManagerForTesting(
      jsTranslateManager);

  // Set up a fake URL for the translate script, to avoid hitting real servers.
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  GURL translateScriptURL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kTranslateScriptPath));
  command_line.AppendSwitchASCII(translate::switches::kTranslateScriptURL,
                                 translateScriptURL.spec().c_str());

  // Translate the page with the link.
  GURL frenchPageURL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPageWithLinkPath));
  [ChromeEarlGrey loadURL:frenchPageURL];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_TRANSLATE_INFOBAR_ACCEPT)]
      performAction:grey_tap()];

  // Check that the translation happened.
  [ChromeEarlGrey waitForWebStateContainingText:"Translated"];

  // Click on the link.
  [ChromeEarlGrey tapWebStateElementWithID:@"link"];
  [ChromeEarlGrey waitForWebStateNotContainingText:"link"];

  GURL frenchPagePathURL = web::test::HttpServer::MakeUrl(
      base::StringPrintf("http://%s", kFrenchPagePath));
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          frenchPagePathURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Check that the auto-translation happened.
  [ChromeEarlGrey waitForWebStateContainingText:"Translated"];
}

#pragma mark - Utility methods

// Simulates translation from Spanish to English, and asserts that the translate
// InfoBar is shown.
- (void)simulateTranslationFromSpanishToEnglish {
  // Simulate translation.
  ChromeIOSTranslateClient* client = ChromeIOSTranslateClient::FromWebState(
      chrome_test_util::GetCurrentWebState());
  client->GetTranslateManager()->PageTranslated(
      "es", "en", translate::TranslateErrors::NONE);

  // The infobar is presented with an animation. Wait for the "Done" button
  // to become visibile before considering the animation as complete.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      ButtonWithAccessibilityLabelId(IDS_DONE)];

  // Assert that the infobar is visible.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(IDS_DONE)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_TRANSLATE_INFOBAR_REVERT)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:CloseButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
