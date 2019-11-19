// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/payments/ios_payment_instrument_finder.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <map>
#include <set>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/payments/core/error_logger.h"
#include "ios/chrome/browser/payments/ios_payment_instrument.h"
#include "ios/chrome/browser/payments/payment_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The maximum number of web app manifests that can be parsed from the payment
// method manifest.
const size_t kMaximumNumberOfWebAppManifests = 100U;

// The following constants are defined in one of the following two documents:
// https://w3c.github.io/payment-method-manifest/
// https://w3c.github.io/manifest/
static const char kDefaultApplications[] = "default_applications";
static const char kShortName[] = "short_name";
static const char kIcons[] = "icons";
static const char kIconsSource[] = "src";
static const char kIconsSizes[] = "sizes";
static const char kIconSizes32[] = "32x32";
static const char kRelatedApplications[] = "related_applications";
static const char kPlatform[] = "platform";
static const char kPlatformItunes[] = "itunes";
static const char kRelatedApplicationsUrl[] = "url";

}  // namespace

namespace payments {

IOSPaymentInstrumentFinder::IOSPaymentInstrumentFinder(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    id<PaymentRequestUIDelegate> payment_request_ui_delegate)
    : downloader_(std::make_unique<ErrorLogger>(), url_loader_factory),
      image_fetcher_(url_loader_factory),
      payment_request_ui_delegate_(payment_request_ui_delegate),
      num_instruments_to_find_(0),
      weak_factory_(this) {}

IOSPaymentInstrumentFinder::~IOSPaymentInstrumentFinder() {}

std::vector<GURL>
IOSPaymentInstrumentFinder::FilterUnsupportedURLPaymentMethods(
    const std::vector<GURL>& queried_url_payment_method_identifiers) {
  const std::map<std::string, std::string>& enum_map =
      payments::GetMethodNameToSchemeName();
  std::vector<GURL> filtered_url_payment_methods;
  for (const GURL& method : queried_url_payment_method_identifiers) {
    // Ensure that the payment method is recognized by looking for an
    // "app-name://" scheme to query for its presence.
    if (!base::Contains(enum_map, method.spec()))
      continue;

    // If there is an app that can handle |scheme| on this device, this payment
    // method is supported.
    const std::string& scheme = enum_map.find(method.spec())->second;
    UIApplication* application = [UIApplication sharedApplication];
    NSURL* URL = [NSURL URLWithString:(base::SysUTF8ToNSString(scheme))];
    if (![application canOpenURL:URL])
      continue;

    filtered_url_payment_methods.push_back(method);
  }

  return filtered_url_payment_methods;
}

std::vector<GURL>
IOSPaymentInstrumentFinder::CreateIOSPaymentInstrumentsForMethods(
    const std::vector<GURL>& methods,
    IOSPaymentInstrumentsFoundCallback callback) {
  // If |callback_| is not null, there's already an active search for iOS
  // payment instruments, which shouldn't happen.
  if (!callback_.is_null()) {
    std::move(callback).Run(
        std::vector<std::unique_ptr<IOSPaymentInstrument>>());
    return std::vector<GURL>();
  }

  // The function should immediately return if there are 0 valid methods
  // supplied.
  const std::vector<GURL>& filtered_methods =
      FilterUnsupportedURLPaymentMethods(methods);
  if (filtered_methods.empty()) {
    std::move(callback).Run(
        std::vector<std::unique_ptr<IOSPaymentInstrument>>());
    return std::vector<GURL>();
  }

  callback_ = std::move(callback);
  // This is originally set to the number of valid payment methods, but may
  // change depending on how many payment apps on a user's device support a
  // particular payment method.
  num_instruments_to_find_ = filtered_methods.size();
  instruments_found_.clear();

  for (const GURL& method : filtered_methods) {
    downloader_.DownloadPaymentMethodManifest(
        method,
        base::BindOnce(&IOSPaymentInstrumentFinder::OnPaymentManifestDownloaded,
                       weak_factory_.GetWeakPtr(), method));
  }

  return filtered_methods;
}

void IOSPaymentInstrumentFinder::OnPaymentManifestDownloaded(
    const GURL& method,
    const GURL& method_url_after_redirects,
    const std::string& content,
    const std::string& error_message) {
  // If |content| is empty then the download failed.
  if (content.empty()) {
    OnPaymentInstrumentProcessed();
    return;
  }

  std::vector<GURL> web_app_manifest_urls;
  // If there are no web app manifests found for the payment method, stop
  // processing this payment method.
  if (!GetWebAppManifestURLsFromPaymentManifest(content,
                                                &web_app_manifest_urls)) {
    OnPaymentInstrumentProcessed();
    return;
  }

  // The payment method manifest can point to several web app manifests.
  // Adjust the expectation of how many instruments we are looking for if
  // this is the case.
  num_instruments_to_find_ += web_app_manifest_urls.size() - 1;

  for (const GURL& web_app_manifest_url : web_app_manifest_urls) {
    downloader_.DownloadWebAppManifest(
        web_app_manifest_url,
        base::BindOnce(&IOSPaymentInstrumentFinder::OnWebAppManifestDownloaded,
                       weak_factory_.GetWeakPtr(), method,
                       web_app_manifest_url));
  }
}

bool IOSPaymentInstrumentFinder::GetWebAppManifestURLsFromPaymentManifest(
    const std::string& input,
    std::vector<GURL>* out_web_app_manifest_urls) {
  DCHECK(out_web_app_manifest_urls->empty());

  std::set<GURL> web_app_manifest_urls;

  base::Optional<base::Value> value = base::JSONReader::Read(input);
  if (!value.has_value()) {
    LOG(ERROR) << "Payment method manifest must be in JSON format.";
    return false;
  }

  if (!value.value().is_dict()) {
    LOG(ERROR) << "Payment method manifest must be a JSON dictionary.";
    return false;
  }

  const base::Value* list = value.value().FindKeyOfType(
      kDefaultApplications, base::Value::Type::LIST);
  if (!list) {
    LOG(ERROR) << "\"" << kDefaultApplications << "\" must be a list.";
    return false;
  }

  base::span<const base::Value> apps = list->GetList();
  if (apps.size() > kMaximumNumberOfWebAppManifests) {
    LOG(ERROR) << "\"" << kDefaultApplications << "\" must contain at most "
               << kMaximumNumberOfWebAppManifests << " entries.";
    return false;
  }

  for (const base::Value& app : apps) {
    if (!app.is_string() || app.GetString().empty())
      continue;

    GURL url(app.GetString());
    if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme))
      continue;

    // Ensure that the json file does not contain any repeated web
    // app manifest URLs.
    const auto result = web_app_manifest_urls.insert(url);
    if (result.second)
      out_web_app_manifest_urls->push_back(url);
  }

  // If we weren't able to find a valid web app manifest URL then we return
  // false.
  return !out_web_app_manifest_urls->empty();
}

void IOSPaymentInstrumentFinder::OnWebAppManifestDownloaded(
    const GURL& method,
    const GURL& web_app_manifest_url,
    const GURL& web_app_manifest_url_after_redirects,
    const std::string& content,
    const std::string& error_message) {
  // If |content| is empty then the download failed.
  if (content.empty()) {
    OnPaymentInstrumentProcessed();
    return;
  }

  std::string app_name;
  GURL app_icon_url;
  GURL universal_link;
  if (!GetPaymentAppDetailsFromWebAppManifest(content, web_app_manifest_url,
                                              &app_name, &app_icon_url,
                                              &universal_link)) {
    OnPaymentInstrumentProcessed();
    return;
  }

  CreateIOSPaymentInstrument(method, app_name, app_icon_url, universal_link);
}

bool IOSPaymentInstrumentFinder::GetPaymentAppDetailsFromWebAppManifest(
    const std::string& input,
    const GURL& web_app_manifest_url,
    std::string* out_app_name,
    GURL* out_app_icon_url,
    GURL* out_universal_link) {
  base::Optional<base::Value> value = base::JSONReader::Read(input);
  if (!value.has_value()) {
    LOG(ERROR) << "Web app manifest must be in JSON format.";
    return false;
  }

  if (!value.value().is_dict()) {
    LOG(ERROR) << "Web app manifest must be a JSON dictionary.";
    return false;
  }

  const std::string* short_name = value.value().FindStringKey(kShortName);
  if (!short_name || short_name->empty()) {
    LOG(ERROR) << "\"" << kShortName << "\" must be a non-empty ASCII string.";
    return false;
  }
  *out_app_name = *short_name;

  const base::Value* list =
      value.value().FindKeyOfType(kIcons, base::Value::Type::LIST);
  if (!list) {
    LOG(ERROR) << "\"" << kIcons << "\" must be a list.";
    return false;
  }

  for (const base::Value& icon : list->GetList()) {
    if (!icon.is_dict())
      continue;

    const std::string* icon_sizes = icon.FindStringKey(kIconsSizes);
    // TODO(crbug.com/752546): Determine acceptable sizes for payment app icon.
    if (!icon_sizes || *icon_sizes != kIconSizes32)
      continue;

    const std::string* icon_string = icon.FindStringKey(kIconsSource);
    if (!icon_string || icon_string->empty())
      continue;

    // The parsed value at "src" may be a relative path such that the base URL
    // is the path to the manifest. If so we check that here.
    GURL complete_url = web_app_manifest_url.Resolve(*icon_string);
    if (complete_url.is_valid() && complete_url.SchemeIs(url::kHttpsScheme)) {
      *out_app_icon_url = complete_url;
      break;
    }

    GURL icon_url(*icon_string);
    if (icon_url.is_valid() && icon_url.SchemeIs(url::kHttpsScheme))
      *out_app_icon_url = icon_url;
  }

  if (out_app_icon_url->is_empty())
    return false;

  list = value.value().FindKeyOfType(kRelatedApplications,
                                     base::Value::Type::LIST);
  if (!list) {
    LOG(ERROR) << "\"" << kRelatedApplications << "\" must be a list.";
    return false;
  }

  for (const base::Value& app : list->GetList()) {
    if (!app.is_dict())
      continue;

    const std::string* platform = app.FindStringKey(kPlatform);
    if (!platform || *platform != kPlatformItunes)
      continue;

    const std::string* link = app.FindStringKey(kRelatedApplicationsUrl);
    if (!link || link->empty())
      continue;

    GURL url(*link);
    if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme))
      continue;

    *out_universal_link = url;
    break;
  }

  return !out_universal_link->is_empty();
}

void IOSPaymentInstrumentFinder::CreateIOSPaymentInstrument(
    const GURL& method_name,
    std::string& app_name,
    GURL& app_icon_url,
    GURL& universal_link) {
  GURL local_method_name(method_name);
  std::string local_app_name(app_name);
  GURL local_universal_link(universal_link);

  image_fetcher::ImageDataFetcherBlock callback =
      ^(NSData* data, const image_fetcher::RequestMetadata& metadata) {
        if (data) {
          UIImage* icon =
              [UIImage imageWithData:data scale:[UIScreen mainScreen].scale];
          instruments_found_.push_back(std::make_unique<IOSPaymentInstrument>(
              local_method_name.spec(), local_universal_link, local_app_name,
              icon, payment_request_ui_delegate_));
        }
        OnPaymentInstrumentProcessed();
      };
  image_fetcher_.FetchImageDataWebpDecoded(app_icon_url, callback);
}

void IOSPaymentInstrumentFinder::OnPaymentInstrumentProcessed() {
  DCHECK(callback_);

  if (--num_instruments_to_find_ == 0)
    std::move(callback_).Run(std::move(instruments_found_));
}

}  // namespace payments
