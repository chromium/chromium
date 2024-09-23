// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/interstitials/interstitial_ui_util.h"

#import "base/atomic_sequence_num.h"
#import "base/check_op.h"
#import "base/memory/ref_counted_memory.h"
#import "base/time/time.h"
#import "components/grit/dev_ui_components_resources.h"
#import "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "components/security_interstitials/core/ssl_error_options_mask.h"
#import "components/security_interstitials/core/unsafe_resource.h"
#import "crypto/rsa_private_key.h"
#import "ios/chrome/browser/safe_browsing/model/safe_browsing_blocking_page.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/ssl/model/ios_captive_portal_blocking_page.h"
#import "ios/chrome/browser/ssl/model/ios_ssl_blocking_page.h"
#import "ios/chrome/browser/webui/ui_bundled/interstitials/interstitial_ui_constants.h"
#import "ios/chrome/browser/webui/ui_bundled/interstitials/interstitial_ui_util.h"
#import "ios/components/security_interstitials/ios_blocking_page_controller_client.h"
#import "ios/components/security_interstitials/ios_blocking_page_metrics_helper.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/webui/url_data_source_ios.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "net/base/url_util.h"
#import "net/cert/x509_certificate.h"
#import "net/cert/x509_util.h"

namespace {

scoped_refptr<net::X509Certificate> CreateFakeCert() {
  // NSS requires that serial numbers be unique even for the same issuer;
  // as all fake certificates will contain the same issuer name, it's
  // necessary to ensure the serial number is unique, as otherwise
  // NSS will fail to parse.
  static base::AtomicSequenceNumber serial_number;

  std::unique_ptr<crypto::RSAPrivateKey> unused_key;
  std::string cert_der;
  if (!net::x509_util::CreateKeyAndSelfSignedCert(
          "CN=Error", static_cast<uint32_t>(serial_number.GetNext()),
          base::Time::Now() - base::Minutes(5),
          base::Time::Now() + base::Minutes(5), &unused_key, &cert_der)) {
    return nullptr;
  }

  return net::X509Certificate::CreateFromBytes(
      base::as_bytes(base::make_span(cert_der)));
}

}

std::unique_ptr<security_interstitials::IOSSecurityInterstitialPage>
CreateSslBlockingPage(web::WebState* web_state, const GURL& url) {
  DCHECK_EQ(kChromeInterstitialSslPath, url.path());
  // Fake parameters for SSL blocking page.
  GURL request_url("https://example.com");
  std::string url_param;
  if (net::GetValueForKeyInQuery(url, kChromeInterstitialSslUrlQueryKey,
                                 &url_param)) {
    GURL query_url_param(url_param);
    if (query_url_param.is_valid())
      request_url = query_url_param;
  }

  bool overridable = false;
  std::string overridable_param;
  if (net::GetValueForKeyInQuery(url, kChromeInterstitialSslOverridableQueryKey,
                                 &overridable_param)) {
    overridable = overridable_param == "1";
  }

  bool strict_enforcement = false;
  std::string strict_enforcement_param;
  if (net::GetValueForKeyInQuery(
          url, kChromeInterstitialSslStrictEnforcementQueryKey,
          &strict_enforcement_param)) {
    strict_enforcement = strict_enforcement_param == "1";
  }

  int cert_error = net::ERR_CERT_CONTAINS_ERRORS;
  std::string type_param;
  if (net::GetValueForKeyInQuery(url, kChromeInterstitialSslTypeQueryKey,
                                 &type_param)) {
    if (type_param == kChromeInterstitialSslTypeHpkpFailureQueryValue) {
      cert_error = net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN;
    } else if (type_param == kChromeInterstitialSslTypeCtFailureQueryValue) {
      cert_error = net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED;
    }
  }

  net::SSLInfo ssl_info;
  ssl_info.cert = ssl_info.unverified_cert = CreateFakeCert();

  int options_mask = 0;
  if (overridable) {
    options_mask |=
        security_interstitials::SSLErrorOptionsMask::SOFT_OVERRIDE_ENABLED;
  }
  if (strict_enforcement) {
    options_mask |=
        security_interstitials::SSLErrorOptionsMask::STRICT_ENFORCEMENT;
  }

  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  reporting_info.metric_prefix =
      overridable ? "ssl_overridable" : "ssl_nonoverridable";

  return std::make_unique<IOSSSLBlockingPage>(
      web_state, cert_error, ssl_info, request_url, options_mask,
      base::Time::NowFromSystemTime(),
      std::make_unique<security_interstitials::IOSBlockingPageControllerClient>(
          web_state,
          std::make_unique<
              security_interstitials::IOSBlockingPageMetricsHelper>(
              web_state, request_url, reporting_info),
          GetApplicationContext()->GetApplicationLocale()));
}

std::unique_ptr<security_interstitials::IOSSecurityInterstitialPage>
CreateCaptivePortalBlockingPage(web::WebState* web_state) {
  GURL landing_url("https://captive.portal/login");
  GURL request_url("https://google.com");

  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  reporting_info.metric_prefix = "ssl_nonoverridable";

  return std::make_unique<IOSCaptivePortalBlockingPage>(
      web_state, request_url, landing_url,
      new security_interstitials::IOSBlockingPageControllerClient(
          web_state,
          std::make_unique<
              security_interstitials::IOSBlockingPageMetricsHelper>(
              web_state, request_url, reporting_info),
          GetApplicationContext()->GetApplicationLocale()));
}

std::unique_ptr<security_interstitials::IOSSecurityInterstitialPage>
CreateSafeBrowsingBlockingPage(web::WebState* web_state, const GURL& url) {
  using enum safe_browsing::SBThreatType;

  safe_browsing::SBThreatType threat_type = SB_THREAT_TYPE_URL_MALWARE;
  GURL request_url("http://example.com");

  // The SafeBrowsingBlockingPage requires the allow list to be instantiated.
  SafeBrowsingUrlAllowList::CreateForWebState(web_state);

  std::string url_param;
  if (net::GetValueForKeyInQuery(
          url, kChromeInterstitialSafeBrowsingUrlQueryKey, &url_param)) {
    GURL query_url_param(url_param);
    if (query_url_param.is_valid())
      request_url = query_url_param;
  }

  std::string type_param;
  if (net::GetValueForKeyInQuery(
          url, kChromeInterstitialSafeBrowsingTypeQueryKey, &type_param)) {
    if (type_param == kChromeInterstitialSafeBrowsingTypeMalwareValue) {
      threat_type = SB_THREAT_TYPE_URL_MALWARE;
    } else if (type_param == kChromeInterstitialSafeBrowsingTypePhishingValue) {
      threat_type = SB_THREAT_TYPE_URL_PHISHING;
    } else if (type_param == kChromeInterstitialSafeBrowsingTypeUnwantedValue) {
      threat_type = SB_THREAT_TYPE_URL_UNWANTED;
    } else if (type_param ==
               kChromeInterstitialSafeBrowsingTypeClientsidePhishingValue) {
      threat_type = SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING;
    } else if (type_param == kChromeInterstitialSafeBrowsingTypeBillingValue) {
      threat_type = SB_THREAT_TYPE_BILLING;
    }
  }

  security_interstitials::UnsafeResource resource;
  resource.url = request_url;
  resource.threat_type = threat_type;
  resource.weak_web_state = web_state->GetWeakPtr();
  // Added to ensure that `threat_source` isn't considered UNKNOWN in this case.
  resource.threat_source = safe_browsing::ThreatSource::LOCAL_PVER4;

  return SafeBrowsingBlockingPage::Create(resource);
}
