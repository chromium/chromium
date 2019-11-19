// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_IOS_PAYMENT_INSTRUMENT_FINDER_H_
#define IOS_CHROME_BROWSER_PAYMENTS_IOS_PAYMENT_INSTRUMENT_FINDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/image_fetcher/ios/ios_image_data_fetcher_wrapper.h"
#include "components/payments/core/payment_manifest_downloader.h"
#include "url/gurl.h"

namespace image_fetcher {
class IOSImageDataFetcherWrapper;
}  // namespace image_fetcher

namespace payments {
class IOSPaymentInstrument;
class PaymentManifestDownloader;
}  // namespace payments

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

@protocol PaymentRequestUIDelegate;

namespace payments {

// Finds all iOS third party native apps for a vector of URL payment method
// identifiers requested by a given merchant and validated by the class. The
// class validates the requested url payment methods by filtering out methods
// that cannot be handled by an installed app on the user's device. This filter
// is done by using the payment method's corresponding URL scheme and the
// canOpenUrl function of UIApplication. This check also serves as a whitelist
// for allowed payment method identifiers such that only the URL schemes of
// listed payment method identifiers can be queried. If the identifier is not on
// this whitelist the validation check fails for that payment method.
//
// After validating the requested payment methods, the class downloads the
// payment method's manifest and web app manifest in order to collect the
// payment app's name, icon, and universal link. If this fails the payment app
// is not deemed to be valid. The manifests are located based on the payment
// method name, which is a URI that starts with "https://".
//
// Example valid web app manifest structure:
//
// {
//   "short_name": "Bobpay",
//   "icons": [{
//     "src": "images/touch/homescreen32.png",
//     "sizes": "32x32"
//   }],
//   "related_applications": [{
//     "platform": "itunes",
//     "url": "https://bobpay.xyz/pay"
//   }]
// }
class IOSPaymentInstrumentFinder {
 public:
  // Callback for when we have tried to find an IOSPaymentInstrument for all
  // requested payment methods. This contains a vector of IOSPaymentInstruments
  // that have valid names, icons, universal links, and method names.
  using IOSPaymentInstrumentsFoundCallback = base::OnceCallback<void(
      std::vector<std::unique_ptr<IOSPaymentInstrument>>)>;

  // Initializes an IOSPaymentInstrumentFinder with a |url_loader_factory| which
  // is used for making URL requests. |payment_request_ui_delegate| is passed to
  // the created IOSPaymentInstrument objects.
  IOSPaymentInstrumentFinder(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      id<PaymentRequestUIDelegate> payment_request_ui_delegate);

  ~IOSPaymentInstrumentFinder();

  // Initiates the process of finding all valid, installed third party native
  // iOS apps and creating corresponding IOSPaymentInstrument representations
  // for these apps. |methods| is a list of URL payment method identifiers
  // requested by the merchant e.g, "https://bobpay.com." |callback| is the
  // IOSPaymentInstrumentsFoundCallback that is run when this class has
  // retrieved all applicable iOS payment instruments. This function returns a
  // filtered version of |methods| which are the url methods that remain after
  // calling FilterUnsupportedURLPaymentMethods.
  std::vector<GURL> CreateIOSPaymentInstrumentsForMethods(
      const std::vector<GURL>& methods,
      IOSPaymentInstrumentsFoundCallback callback);

 private:
  friend class PaymentRequestIOSPaymentInstrumentFinderTest;

  // Filters out |queried_url_payment_method_identifiers| for any invalid
  // url payment method identifiers queried by the caller. An invalid url
  // payment method identifier is one that doesn't have a corresponding
  // "app-name:// scheme" or one that doesn't have a corresponding installed
  // payment app.
  virtual std::vector<GURL> FilterUnsupportedURLPaymentMethods(
      const std::vector<GURL>& queried_url_payment_method_identifiers);

  // Callback for when the payment method manifest is retrieved for a payment
  // method identifier. |content| is the json encoded payment method manifest
  // and |method| is the url payment method identifier that corresponds with
  // this manifest.
  void OnPaymentManifestDownloaded(const GURL& method,
                                   const GURL& method_url_after_redirects,
                                   const std::string& content,
                                   const std::string& error_message);

  // Parses a payment method manifest for its default applications and gets all
  // the valid ones. |input| is the json encoded payment method manifest to
  // parse. |out_web_app_manifest_urls| is a vector of the web app manifest
  // URLs of valid payment apps if any exist.
  bool GetWebAppManifestURLsFromPaymentManifest(
      const std::string& input,
      std::vector<GURL>* out_web_app_manifest_urls);

  // Callback for when the web app manifest is retrieved for a payment method
  // identifier. |content| is the json encoded web app manifest and |method| is
  // the url payment method that corresponds with this manifest.
  // |web_app_manifest_url| is the web app manifest url. This is passed to
  // GetPaymentAppDetailsFromWebAppManifest for validating an icon source path.
  void OnWebAppManifestDownloaded(
      const GURL& method,
      const GURL& web_app_manifest_url,
      const GURL& web_app_manifest_url_after_redirects,
      const std::string& content,
      const std::string& error_message);

  // Parses a web app manifest for its name, icon, and universal link. |input|
  // is the json encoded web app manifest to parse. |web_app_manifest_url|
  // is the web app manifest url which is needed for determining if the relative
  // path to an icon image source is valid. |out_app_name|, |out_app_icon_url|,
  // and |out_universal_link| are set to the name, icon url, and universal link
  // of a payment app.
  bool GetPaymentAppDetailsFromWebAppManifest(const std::string& input,
                                              const GURL& web_app_manifest_url,
                                              std::string* out_app_name,
                                              GURL* out_app_icon_url,
                                              GURL* out_universal_link);

  // Creates an instance of IOSPaymentInstrument and appends it to
  // |instruments_found_|.
  void CreateIOSPaymentInstrument(const GURL& method_name,
                                  std::string& app_name,
                                  GURL& app_icon_url,
                                  GURL& universal_link);

  // Whenever the class attempts to find an instrument for a given method it
  // will either succeed or fail; in either scenario this method will decrease
  // the number of payment instruments that still need to be addressed which
  // is tracked with |num_instruments_to_find_|. When this number reaches 0 the
  // class calls the IOSPaymentInstrumentsFound callback.
  void OnPaymentInstrumentProcessed();

  PaymentManifestDownloader downloader_;
  image_fetcher::IOSImageDataFetcherWrapper image_fetcher_;

  __weak id<PaymentRequestUIDelegate> payment_request_ui_delegate_;

  size_t num_instruments_to_find_;
  std::vector<std::unique_ptr<IOSPaymentInstrument>> instruments_found_;
  IOSPaymentInstrumentsFoundCallback callback_;

  base::WeakPtrFactory<IOSPaymentInstrumentFinder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IOSPaymentInstrumentFinder);
};

}  // namespace payments

#endif  // IOS_CHROME_BROWSER_PAYMENTS_IOS_PAYMENT_INSTRUMENT_FINDER_H_
