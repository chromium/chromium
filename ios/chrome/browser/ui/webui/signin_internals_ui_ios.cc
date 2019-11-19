// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/signin_internals_ui_ios.h"

#include "base/hash/hash.h"
#include "components/grit/components_resources.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/signin/about_signin_internals_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_data_source.h"

namespace {

web::WebUIIOSDataSource* CreateSignInInternalsHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUISignInInternalsHost);

  source->UseStringsJs();
  source->AddResourcePath("signin_internals.js", IDR_SIGNIN_INTERNALS_INDEX_JS);
  source->SetDefaultResource(IDR_SIGNIN_INTERNALS_INDEX_HTML);

  return source;
}

}  //  namespace

SignInInternalsUIIOS::SignInInternalsUIIOS(web::WebUIIOS* web_ui)
    : WebUIIOSController(web_ui) {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromWebUIIOS(web_ui);
  DCHECK(browser_state);
  web::WebUIIOSDataSource::Add(browser_state,
                               CreateSignInInternalsHTMLSource());

  AboutSigninInternals* about_signin_internals =
      ios::AboutSigninInternalsFactory::GetForBrowserState(browser_state);
  if (about_signin_internals)
    about_signin_internals->AddSigninObserver(this);
}

SignInInternalsUIIOS::~SignInInternalsUIIOS() {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromWebUIIOS(web_ui());
  DCHECK(browser_state);
  AboutSigninInternals* about_signin_internals =
      ios::AboutSigninInternalsFactory::GetForBrowserState(browser_state);
  if (about_signin_internals)
    about_signin_internals->RemoveSigninObserver(this);
}

bool SignInInternalsUIIOS::OverrideHandleWebUIIOSMessage(
    const GURL& source_url,
    const std::string& name,
    const base::ListValue& content) {
  if (name == "getSigninInfo") {
    ios::ChromeBrowserState* browser_state =
        ios::ChromeBrowserState::FromWebUIIOS(web_ui());
    DCHECK(browser_state);

    AboutSigninInternals* about_signin_internals =
        ios::AboutSigninInternalsFactory::GetForBrowserState(browser_state);
    // TODO(vishwath): The UI would look better if we passed in a dict with some
    // reasonable defaults, so the about:signin-internals page doesn't look
    // empty in incognito mode. Alternatively, we could force about:signin to
    // open in non-incognito mode always (like about:settings for ex.).
    if (about_signin_internals) {
      base::Value status = about_signin_internals->GetSigninStatus()->Clone();
      std::vector<const base::Value*> args{&status};
      web_ui()->CallJavascriptFunction(
          "chrome.signin.getSigninInfo.handleReply", args);
      signin::IdentityManager* identity_manager =
          IdentityManagerFactory::GetForBrowserState(browser_state);
      signin::AccountsInCookieJarInfo accounts_in_cookie_jar =
          identity_manager->GetAccountsInCookieJar();
      if (accounts_in_cookie_jar.accounts_are_fresh) {
        about_signin_internals->OnAccountsInCookieUpdated(
            accounts_in_cookie_jar,
            GoogleServiceAuthError(GoogleServiceAuthError::NONE));
      }

      return true;
    }
  }
  return false;
}

void SignInInternalsUIIOS::OnSigninStateChanged(
    const base::DictionaryValue* info) {
  std::vector<const base::Value*> args{info};
  web_ui()->CallJavascriptFunction("chrome.signin.onSigninInfoChanged.fire",
                                   args);
}

void SignInInternalsUIIOS::OnCookieAccountsFetched(
    const base::DictionaryValue* info) {
  std::vector<const base::Value*> args{info};
  web_ui()->CallJavascriptFunction("chrome.signin.onCookieAccountsFetched.fire",
                                   args);
}
