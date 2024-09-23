// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_site_security_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/security_state/core/security_state.h"
#import "components/ssl_errors/error_info.h"
#import "components/strings/grit/components_branded_strings.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/reading_list/model/offline_page_tab_helper.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/page_info/features.h"
#import "ios/chrome/browser/ui/page_info/page_info_constants.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_description.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/security/ssl_status.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

// Build the certificate details based on the `SSLStatus` and the `URL`.
NSString* BuildCertificateDetailString(web::SSLStatus& SSLStatus,
                                       const GURL& URL) {
  NSMutableString* certificateDetails = [NSMutableString
      stringWithString:l10n_util::GetNSString(
                           IDS_PAGE_INFO_SECURITY_TAB_INSECURE_IDENTITY)];
  NSString* bullet = @"\n • ";
  std::vector<ssl_errors::ErrorInfo> errors;
  ssl_errors::ErrorInfo::GetErrorsForCertStatus(
      SSLStatus.certificate, SSLStatus.cert_status, URL, &errors);
  for (size_t i = 0; i < errors.size(); ++i) {
    [certificateDetails appendString:bullet];
    [certificateDetails
        appendString:base::SysUTF16ToNSString(errors[i].short_description())];
  }

  return certificateDetails;
}

// Returns a messages, based on the `messagesComponents`, joined by a spacing.
NSString* BuildMessage(NSArray<NSString*>* messageComponents) {
  DCHECK(messageComponents.count > 0);
  NSMutableString* message =
      [NSMutableString stringWithString:messageComponents[0]];
  for (NSUInteger index = 1; index < messageComponents.count; index++) {
    NSString* component = messageComponents[index];
    if (component.length == 0)
      continue;
    [message appendString:@"\n\n"];
    [message appendString:component];
  }
  return message;
}

}  // namespace

@implementation PageInfoSiteSecurityMediator

+ (PageInfoSiteSecurityDescription*)configurationForWebState:
    (web::WebState*)webState {
  web::NavigationItem* navItem =
      webState->GetNavigationManager()->GetVisibleItem();
  const GURL& URL = navItem->GetURL();
  web::SSLStatus& status = navItem->GetSSL();
  bool offlinePage =
      OfflinePageTabHelper::FromWebState(webState)->presenting_offline_page();

  PageInfoSiteSecurityDescription* dataHolder =
      [[PageInfoSiteSecurityDescription alloc] init];

  if (offlinePage) {
    dataHolder.siteURL =
        l10n_util::GetNSString(IDS_IOS_PAGE_INFO_OFFLINE_PAGE_LABEL);

    dataHolder.message = l10n_util::GetNSString(IDS_IOS_PAGE_INFO_OFFLINE_PAGE);
    dataHolder.isEmpty = YES;
    return dataHolder;
  }

  if (URL.SchemeIs(kChromeUIScheme)) {
    dataHolder.siteURL =
        l10n_util::GetNSString(IDS_IOS_PAGE_INFO_CHROME_PAGE_LABEL);
    dataHolder.message = l10n_util::GetNSString(IDS_PAGE_INFO_INTERNAL_PAGE);
    dataHolder.isEmpty = YES;
    return dataHolder;
  }

  // At this point, this is a web page.
  dataHolder.siteURL = base::SysUTF8ToNSString(URL.host());
  dataHolder.isEmpty = NO;
  dataHolder.status =
      l10n_util::GetNSString(IDS_IOS_PAGE_INFO_SECURITY_STATUS_NOT_SECURE);
  dataHolder.securityStatus = l10n_util::GetNSString(
      IDS_IOS_PAGE_INFO_SECURITY_CONNECTION_STATUS_NOT_SECURE);
  dataHolder.secure = NO;
  dataHolder.isPageLoading = webState->IsLoading();

  // Summary and details.
  if (!status.certificate) {
    // Not HTTPS. This maps to the WARNING security level. Show the red
    // triangle icon in page info based on the same logic used to determine
    // the iconography in the omnibox.
    dataHolder.iconImage = DefaultSymbolTemplateWithPointSize(
        kWarningSymbol, kPageInfoSymbolPointSize);
    dataHolder.iconBackgroundColor = [UIColor colorNamed:kRed500Color];

    if (IsRevampPageInfoIosEnabled()) {
      dataHolder.message =
          l10n_util::GetNSString(IDS_PAGE_INFO_NOT_SECURE_DETAILS);
    } else {
      dataHolder.message =
          [NSString stringWithFormat:@"%@ BEGIN_LINK %@ END_LINK",
                                     l10n_util::GetNSString(
                                         IDS_PAGE_INFO_NOT_SECURE_DETAILS),
                                     l10n_util::GetNSString(IDS_LEARN_MORE)];
    }

    return dataHolder;
  }

  // It is possible to have `SECURITY_STYLE_AUTHENTICATION_BROKEN` and non-error
  // `cert_status` for WKWebView because `security_style` and `cert_status`
  // are
  // calculated using different API, which may lead to different cert
  // verification results.
  if (net::IsCertStatusError(status.cert_status) ||
      status.security_style == web::SECURITY_STYLE_AUTHENTICATION_BROKEN) {
    // HTTPS with major errors
    dataHolder.iconImage = DefaultSymbolTemplateWithPointSize(
        kWarningSymbol, kPageInfoSymbolPointSize);
    dataHolder.iconBackgroundColor = [UIColor colorNamed:kRed500Color];

    NSString* certificateDetails = BuildCertificateDetailString(status, URL);

    if (IsRevampPageInfoIosEnabled()) {
      dataHolder.message = BuildMessage(@[
        l10n_util::GetNSString(IDS_PAGE_INFO_NOT_SECURE_DETAILS),
        certificateDetails
      ]);
    } else {
      dataHolder.message = BuildMessage(@[
        [NSString stringWithFormat:@"%@ BEGIN_LINK %@ END_LINK",
                                   l10n_util::GetNSString(
                                       IDS_PAGE_INFO_NOT_SECURE_DETAILS),
                                   l10n_util::GetNSString(IDS_LEARN_MORE)],
        certificateDetails
      ]);
    }

    return dataHolder;
  }

  // The remaining states are valid HTTPS, or HTTPS with minor errors.

  std::u16string issuerName(
      base::UTF8ToUTF16(status.certificate->issuer().GetDisplayName()));
  // Have certificateDetails be an empty string to help building the message.
  NSString* certificateDetails = @"";
  if (!issuerName.empty()) {
    // Show the issuer name if it's available.
    // TODO(crbug.com/41183995): Implement a certificate viewer instead.
    certificateDetails = l10n_util::GetNSStringF(
        IDS_IOS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY, issuerName);
  }

  if (status.content_status == web::SSLStatus::DISPLAYED_INSECURE_CONTENT) {
    // HTTPS with mixed content. This maps to the WARNING security level in M80,
    // so assume the WARNING state when determining whether to swap the icon for
    // a red triangle. This will result in an inconsistency between the omnibox
    // and page info if the mixed content WARNING feature is disabled.
    dataHolder.iconImage = DefaultSymbolTemplateWithPointSize(
        kWarningSymbol, kPageInfoSymbolPointSize);
    dataHolder.iconBackgroundColor = [UIColor colorNamed:kRed500Color];

    if (IsRevampPageInfoIosEnabled()) {
      dataHolder.message = BuildMessage(@[
        l10n_util::GetNSString(IDS_PAGE_INFO_MIXED_CONTENT_DETAILS),
        certificateDetails
      ]);
    } else {
      dataHolder.message = BuildMessage(@[
        [NSString stringWithFormat:@"%@ BEGIN_LINK %@ END_LINK",
                                   l10n_util::GetNSString(
                                       IDS_PAGE_INFO_MIXED_CONTENT_DETAILS),
                                   l10n_util::GetNSString(IDS_LEARN_MORE)],
        certificateDetails
      ]);
    }

    return dataHolder;
  }

  // Valid HTTPS
  dataHolder.status =
      l10n_util::GetNSString(IDS_IOS_PAGE_INFO_SECURITY_STATUS_SECURE);
  dataHolder.securityStatus = l10n_util::GetNSString(
      IDS_IOS_PAGE_INFO_SECURITY_CONNECTION_STATUS_SECURE);
  dataHolder.secure = YES;
  dataHolder.iconImage = IsRevampPageInfoIosEnabled()
                             ? DefaultSymbolTemplateWithPointSize(
                                   kSecureSymbol, kPageInfoSymbolPointSize)
                             : nil;
  dataHolder.iconBackgroundColor = [UIColor colorNamed:kGreen500Color];

  if (IsRevampPageInfoIosEnabled()) {
    dataHolder.message = BuildMessage(@[
      l10n_util::GetNSString(IDS_PAGE_INFO_SECURE_DETAILS), certificateDetails
    ]);
  } else {
    dataHolder.message = BuildMessage(@[
      [NSString
          stringWithFormat:@"%@ BEGIN_LINK %@ END_LINK",
                           l10n_util::GetNSString(IDS_PAGE_INFO_SECURE_DETAILS),
                           l10n_util::GetNSString(IDS_LEARN_MORE)],
      certificateDetails
    ]);
  }

  DCHECK(!(status.cert_status & net::CERT_STATUS_IS_EV))
      << "Extended Validation should be disabled";

  return dataHolder;
}

@end
