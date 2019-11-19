// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/page_info/page_info_model.h"

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/security_state/core/security_state.h"
#include "components/ssl_errors/error_info.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_google_chrome_strings.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/ui/page_info/page_info_model_observer.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/web/common/origin_util.h"
#include "ios/web/public/security/ssl_status.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

// TODO(crbug.com/227827) Merge 178763: PageInfoModel has been removed in
// upstream; check if we should use PageInfoModel.
PageInfoModel::PageInfoModel(ios::ChromeBrowserState* browser_state,
                             const GURL& url,
                             const web::SSLStatus& ssl,
                             bool is_offline_page,
                             PageInfoModelObserver* observer)
    : observer_(observer) {
  if (is_offline_page) {
    sections_.push_back(
        SectionInfo(ICON_STATE_OFFLINE_PAGE,
                    l10n_util::GetStringUTF16(IDS_IOS_PAGE_INFO_OFFLINE_TITLE),
                    l10n_util::GetStringUTF16(IDS_IOS_PAGE_INFO_OFFLINE_PAGE),
                    SECTION_INFO_INTERNAL_PAGE, BUTTON_RELOAD));
    return;
  }

  if (url.SchemeIs(kChromeUIScheme)) {
    base::string16 spec(base::UTF8ToUTF16(url.spec()));

    sections_.push_back(
        SectionInfo(ICON_STATE_INTERNAL_PAGE, spec,
                    l10n_util::GetStringUTF16(IDS_PAGE_INFO_INTERNAL_PAGE),
                    SECTION_INFO_INTERNAL_PAGE, BUTTON_NONE));
    return;
  }

  base::string16 hostname(base::UTF8ToUTF16(url.host()));

  base::string16 summary;
  base::string16 details;
  base::string16 certificate_details;

  // Summary and details.
  SectionStateIcon icon_id = ICON_NONE;
  if (!ssl.certificate) {
    // Not HTTPS. This maps to the WARNING security level. Show the grey
    // triangle icon in page info based on the same logic used to determine
    // the iconography in the omnibox.
    if (security_state::ShouldDowngradeNeutralStyling(
            security_state::SecurityLevel::WARNING, url,
            base::BindRepeating(&web::IsOriginSecure))) {
      icon_id = ICON_STATE_ERROR;
    } else {
      icon_id = ICON_STATE_INFO;
    }
    summary.assign(l10n_util::GetStringUTF16(IDS_PAGE_INFO_NOT_SECURE_SUMMARY));
    details.assign(l10n_util::GetStringUTF16(IDS_PAGE_INFO_NOT_SECURE_DETAILS));
  } else {
    // It is possible to have |SECURITY_STYLE_AUTHENTICATION_BROKEN| and
    // non-error
    // |cert_status| for WKWebView because |security_style| and |cert_status|
    // are
    // calculated using different API, which may lead to different cert
    // verification results.
    if (net::IsCertStatusError(ssl.cert_status) ||
        ssl.security_style == web::SECURITY_STYLE_AUTHENTICATION_BROKEN) {
      // HTTPS with major errors
      icon_id = ICON_STATE_ERROR;
      summary.assign(
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_NOT_SECURE_SUMMARY));
      details.assign(
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_NOT_SECURE_DETAILS));

      certificate_details.assign(l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_INSECURE_IDENTITY));
      const base::string16 bullet = base::UTF8ToUTF16("\n • ");
      std::vector<ssl_errors::ErrorInfo> errors;
      ssl_errors::ErrorInfo::GetErrorsForCertStatus(
          ssl.certificate, ssl.cert_status, url, &errors);
      for (size_t i = 0; i < errors.size(); ++i) {
        certificate_details += bullet;
        certificate_details += errors[i].short_description();
      }
    } else {
      // Valid HTTPS or HTTPS with minor errors.
      base::string16 issuer_name(
          base::UTF8ToUTF16(ssl.certificate->issuer().GetDisplayName()));
      if (!issuer_name.empty()) {
        // Show the issuer name if it's available.
        // TODO(crbug.com/502470): Implement a certificate viewer instead.
        certificate_details.assign(l10n_util::GetStringFUTF16(
            IDS_IOS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY, issuer_name));
      }
      if (ssl.content_status == web::SSLStatus::DISPLAYED_INSECURE_CONTENT) {
        // HTTPS with mixed content. This maps to the NONE security level. Show
        // the grey triangle icon in page info based on the same logic used to
        // determine the iconography in the omnibox.
        if (security_state::ShouldDowngradeNeutralStyling(
                security_state::SecurityLevel::NONE, url,
                base::BindRepeating(&web::IsOriginSecure))) {
          icon_id = ICON_STATE_ERROR;
        } else {
          icon_id = ICON_STATE_INFO;
        }
        summary.assign(
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_MIXED_CONTENT_SUMMARY));
        details.assign(
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_MIXED_CONTENT_DETAILS));
      } else {
        // Valid HTTPS
        icon_id = ICON_STATE_OK;
        summary.assign(l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURE_SUMMARY));
        details.assign(l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURE_DETAILS));

        DCHECK(!(ssl.cert_status & net::CERT_STATUS_IS_EV))
            << "Extended Validation should be disabled";
      }
    }
  }

  base::string16 description;
  base::string16 spacer = base::UTF8ToUTF16("\n\n");

  description.assign(summary);
  description += spacer;
  description += details;

  if (!certificate_details.empty()) {
    description += spacer;
    description += certificate_details;
  }

  sections_.push_back(SectionInfo(icon_id, hostname, description,
                                  SECTION_INFO_CONNECTION,
                                  BUTTON_SHOW_SECURITY_HELP));
}

PageInfoModel::~PageInfoModel() {}

int PageInfoModel::GetSectionCount() {
  return sections_.size();
}

PageInfoModel::SectionInfo PageInfoModel::GetSectionInfo(int index) {
  DCHECK(index < static_cast<int>(sections_.size()));
  return sections_[index];
}

gfx::Image* PageInfoModel::GetIconImage(SectionStateIcon icon_id) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  switch (icon_id) {
    case ICON_NONE:
    case ICON_STATE_INTERNAL_PAGE:
      return nullptr;
    case ICON_STATE_OK:
      return &rb.GetNativeImageNamed(IDR_IOS_PAGEINFO_GOOD);
    case ICON_STATE_ERROR:
      return &rb.GetNativeImageNamed(IDR_IOS_PAGEINFO_BAD);
    case ICON_STATE_INFO:
      return &rb.GetNativeImageNamed(IDR_IOS_PAGEINFO_INFO);
    case ICON_STATE_OFFLINE_PAGE:
      return &rb.GetNativeImageNamed(IDR_IOS_OMNIBOX_OFFLINE);
  }
}

base::string16 PageInfoModel::GetCertificateLabel() const {
  return certificate_label_;
}

PageInfoModel::PageInfoModel() : observer_(NULL) {}
