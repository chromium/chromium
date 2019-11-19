// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/payments/ios_payment_instrument_finder.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ios/chrome/browser/payments/ios_payment_instrument.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace payments {

class TestIOSPaymentInstrumentFinder final : public IOSPaymentInstrumentFinder {
 public:
  TestIOSPaymentInstrumentFinder(
      scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory)
      : IOSPaymentInstrumentFinder(test_url_loader_factory, nil) {}

  std::vector<GURL> FilterUnsupportedURLPaymentMethods(
      const std::vector<GURL>& queried_url_payment_method_identifiers)
      override {
    return queried_url_payment_method_identifiers;
  }

  DISALLOW_COPY_AND_ASSIGN(TestIOSPaymentInstrumentFinder);
};

class PaymentRequestIOSPaymentInstrumentFinderTest : public PlatformTest {
 public:
  PaymentRequestIOSPaymentInstrumentFinderTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        shared_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        ios_payment_instrument_finder_(
            std::make_unique<TestIOSPaymentInstrumentFinder>(shared_factory_)) {
  }

  ~PaymentRequestIOSPaymentInstrumentFinderTest() override {}

  size_t num_instruments_to_find() {
    return ios_payment_instrument_finder_->num_instruments_to_find_;
  }

  const std::vector<std::unique_ptr<IOSPaymentInstrument>>& result() {
    return result_;
  }

  void ExpectUnableToParsePaymentMethodManifest(const std::string& input) {
    std::vector<GURL> actual_web_app_urls;

    bool success = ios_payment_instrument_finder_
                       ->GetWebAppManifestURLsFromPaymentManifest(
                           input, &actual_web_app_urls);

    EXPECT_FALSE(success);
    EXPECT_TRUE(actual_web_app_urls.empty());
  }

  void ExpectParsedPaymentMethodManifest(
      const std::string& input,
      const std::vector<GURL>& expected_web_app_url) {
    std::vector<GURL> actual_web_app_urls;

    bool success = ios_payment_instrument_finder_
                       ->GetWebAppManifestURLsFromPaymentManifest(
                           input, &actual_web_app_urls);

    EXPECT_TRUE(success);
    EXPECT_EQ(expected_web_app_url, actual_web_app_urls);
  }

  void ExpectUnableToParseWebAppManifest(const std::string& input) {
    std::string actual_app_name;
    GURL actual_app_icon;
    GURL actual_universal_link;

    bool success =
        ios_payment_instrument_finder_->GetPaymentAppDetailsFromWebAppManifest(
            input, GURL("https://bobpay.xyz/bob/manifest.json"),
            &actual_app_name, &actual_app_icon, &actual_universal_link);

    EXPECT_FALSE(success);
    EXPECT_TRUE(actual_app_name.empty() || actual_app_icon.is_empty() ||
                actual_universal_link.is_empty());
  }

  void ExpectParsedWebAppManifest(const std::string& input,
                                  const std::string& expected_app_name,
                                  const GURL& expected_app_icon,
                                  const GURL& expected_universal_link) {
    std::string actual_app_name;
    GURL actual_app_icon;
    GURL actual_universal_link;

    bool success =
        ios_payment_instrument_finder_->GetPaymentAppDetailsFromWebAppManifest(
            input, GURL("https://bobpay.xyz/bob/manifest.json"),
            &actual_app_name, &actual_app_icon, &actual_universal_link);

    EXPECT_TRUE(success);
    EXPECT_EQ(expected_app_name, actual_app_name);
    EXPECT_EQ(expected_app_icon, actual_app_icon);
    EXPECT_EQ(expected_universal_link, actual_universal_link);
  }

  // A callback method for testing if and when the iOS payment instrument
  // finder finishes searching for native third party payment apps.
  void InstrumentsFoundCallback(
      std::vector<std::unique_ptr<IOSPaymentInstrument>> result) {
    result_ = std::move(result);
    if (run_loop_)
      run_loop_->Quit();
  }

  void FindInstrumentsWithMethods(std::vector<GURL>& url_methods) {
    ios_payment_instrument_finder_->CreateIOSPaymentInstrumentsForMethods(
        url_methods,
        base::BindOnce(&PaymentRequestIOSPaymentInstrumentFinderTest::
                           InstrumentsFoundCallback,
                       base::Unretained(this)));
  }

  void FindInstrumentsWithWebAppManifest(const GURL& method,
                                         const std::string& content) {
    ios_payment_instrument_finder_->callback_ = base::BindOnce(
        &PaymentRequestIOSPaymentInstrumentFinderTest::InstrumentsFoundCallback,
        base::Unretained(this));
    ios_payment_instrument_finder_->num_instruments_to_find_ = 1;
    GURL web_app_manifest_url("https://bobpay.xyz/bob/manifest.json");
    ios_payment_instrument_finder_->OnWebAppManifestDownloaded(
        method, web_app_manifest_url, web_app_manifest_url, content,
        /*error_message=*/"");
  }

  void RunLoop() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;

  std::unique_ptr<TestIOSPaymentInstrumentFinder>
      ios_payment_instrument_finder_;

  std::unique_ptr<base::RunLoop> run_loop_;
  std::vector<std::unique_ptr<IOSPaymentInstrument>> result_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestIOSPaymentInstrumentFinderTest);
};

// Payment method manifest parsing:

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       NullPaymentMethodManifestIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest(std::string());
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       NonJsonPaymentMethodManifestIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("this is not json");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       StringPaymentMethodManifestIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("\"this is a string\"");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       EmptyDictionaryPaymentMethodManifestIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("{}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       NullDefaultApplicationIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("{\"default_applications\": null}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       NumberDefaultApplicationIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("{\"default_applications\": 0}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       ListOfNumbersDefaultApplicationIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("{\"default_applications\": [0]}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       EmptyListOfDefaultApplicationsIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("{\"default_applications\": []}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       ListOfEmptyDefaultApplicationsIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"default_applications\": [\"\"]}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       DefaultApplicationsShouldNotHaveNulCharacters) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"default_applications\": [\"https://bobpay.com/app\0json\"]}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       DefaultApplicationKeyShouldBeLowercase) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"Default_Applications\": [\"https://bobpay.com/app.json\"]}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       DefaultApplicationsShouldHaveAbsoluteUrl) {
  ExpectUnableToParsePaymentMethodManifest("{\"default_applications\": ["
                                           "\"app.json\"]}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       DefaultApplicationsShouldBeHttps) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"default_applications\": ["
      "\"http://bobpay.com/app.json\","
      "\"http://alicepay.com/app.json\"]}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       WellFormedPaymentMethodManifestWithApps) {
  ExpectParsedPaymentMethodManifest("{\"default_applications\": ["
                                    "\"https://bobpay.com/app.json\","
                                    "\"https://alicepay.com/app.json\"]}",
                                    {GURL("https://bobpay.com/app.json"),
                                     GURL("https://alicepay.com/app.json")});
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       WellFormedPaymentMethodManifestWithDuplicateApps) {
  ExpectParsedPaymentMethodManifest("{\"default_applications\": ["
                                    "\"https://bobpay.com/app.json\","
                                    "\"https://bobpay.com/app.json\","
                                    "\"https://alicepay.com/app.json\"]}",
                                    {GURL("https://bobpay.com/app.json"),
                                     GURL("https://alicepay.com/app.json")});
}

// Web app manifest parsing:

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest, NullContentIsMalformed) {
  ExpectUnableToParseWebAppManifest(std::string());
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       NonJsonContentIsMalformed) {
  ExpectUnableToParseWebAppManifest("this is not json");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest, StringContentIsMalformed) {
  ExpectUnableToParseWebAppManifest("\"this is a string\"");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       EmptyDictionaryIsMalformed) {
  ExpectUnableToParseWebAppManifest("{}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       NullRelatedApplicationsSectionIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/bob/images/homescreen32.png\", "
      "    \"sizes\": \"32x32\""
      "  }], "
      "  \"related_applications\": null");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       NumberRelatedApplicationSectionIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/bob/images/homescreen32.png\", "
      "    \"sizes\": \"32x32\""
      "  }], "
      "  \"related_applications\": 0"
      "}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       ListOfNumbersRelatedApplicationsSectionIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/bob/images/homescreen32.png\", "
      "    \"sizes\": \"32x32\""
      "  }], "
      "  \"related_applications\": [0]"
      "}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       EmptyListRelatedApplicationsSectionIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/bob/images/homescreen32.png\", "
      "    \"sizes\": \"32x32\""
      "  }], "
      "  \"related_applications\": []"
      "}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       ListOfEmptyDictionariesRelatedApplicationsSectionIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/bob/images/homescreen32.png\", "
      "    \"sizes\": \"32x32\""
      "  }], "
      "  \"related_applications\": [{}]"
      "}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       NoItunesPlatformIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/bob/images/homescreen32.png\", "
      "    \"sizes\": \"32x32\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"url\": \"https://bobpay.xyz/pay\""
      "  }]"
      "}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       NoUniversalLinkIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/bob/images/homescreen32.png\", "
      "    \"sizes\": \"32x32\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"itunes\""
      "  }]"
      "}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest, NoShortNameIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/bob/images/homescreen32.png\", "
      "    \"sizes\": \"32x32\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"itunes\""
      "  }]"
      "}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       PlatformShouldNotHaveNullCharacters) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/bob/images/homescreen32.png\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"it\0unes\", "
      "    \"url\": \"https://bobpay.xyz/pay\""
      "  }]"
      "}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       UniversalLinkShouldNotHaveNullCharacters) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/bob/images/homescreen32.png\", "
      "    \"sizes\": \"32x32\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"itunes\", "
      "    \"url\": \"https://bobp\0ay.xyz/pay\""
      "  }]"
      "}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       IconSourceShouldNotHaveNullCharacters) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/bob/ima\0ges/homescreen32.png\", "
      "    \"sizes\": \"32x32\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"itunes\", "
      "    \"url\": \"https://bobpay.xyz/pay\""
      "  }]"
      "}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       IconSizesShouldNotHaveNullCharacters) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/bob/images/homescreen32.png\", "
      "    \"sizes\": \"32x\032\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"itunes\", "
      "    \"url\": \"https://bobpay.xyz/pay\""
      "  }]"
      "}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       ShortNameShouldNotHaveNullCharacters) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bob\0pay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/bob/images/homescreen32.png\", "
      "    \"sizes\": \"32x32\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"itunes\", "
      "    \"url\": \"https://bobpay.xyz/pay\""
      "  }]"
      "}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest, KeysShouldBeLowerCase) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"Short_name\": \"Bobpay\", "
      "  \"Icons\": [{"
      "    \"Src\": \"https://bobpay.xyz/bob/images/homescreen32.png\", "
      "    \"Sizes\": \"32x32\""
      "  }], "
      "  \"Related_applications\": [{"
      "    \"Platform\": \"itunes\", "
      "    \"Url\": \"https://bobpay.xyz/pay\""
      "  }]"
      "}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       UniversalLinkShouldHaveAbsoluteUrl) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/bob/images/homescreen32.png\", "
      "    \"sizes\": \"32x32\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"itunes\", "
      "    \"url\": \"pay.html\""
      "  }]"
      "}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       UniversalLinkShouldBeHttps) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/bob/images/homescreen32.png\", "
      "    \"sizes\": \"32x32\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"itunes\", "
      "    \"url\": \"http://bobpay.xyz/pay\""
      "  }]"
      "}");
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest, WellFormed) {
  ExpectParsedWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/bob/images/homescreen32.png\", "
      "    \"sizes\": \"32x32\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"itunes\", "
      "    \"url\": \"https://bobpay.xyz/pay\""
      "  }]"
      "}",
      "Bobpay", GURL("https://bobpay.xyz/bob/images/homescreen32.png"),
      GURL("https://bobpay.xyz/pay"));
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       RelativeIconPathWellFormed) {
  ExpectParsedWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"images/homescreen32.png\", "
      "    \"sizes\": \"32x32\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"itunes\", "
      "    \"url\": \"https://bobpay.xyz/pay\""
      "  }]"
      "}",
      "Bobpay", GURL("https://bobpay.xyz/bob/images/homescreen32.png"),
      GURL("https://bobpay.xyz/pay"));
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       RelativeIconPathForwardSlashWellFormed) {
  ExpectParsedWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"/bob2/images/homescreen32.png\", "
      "    \"sizes\": \"32x32\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"itunes\", "
      "    \"url\": \"https://bobpay.xyz/pay\""
      "  }]"
      "}",
      "Bobpay", GURL("https://bobpay.xyz/bob2/images/homescreen32.png"),
      GURL("https://bobpay.xyz/pay"));
}

TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       TwoRelatedApplicationsSecondIsWellFormed) {
  ExpectParsedWebAppManifest(
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/bob/images/homescreen32.png\", "
      "    \"sizes\": \"32x32\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"url\": \"https://bobpay.xyz/pay\""
      "  }, {"
      "    \"platform\": \"itunes\", "
      "    \"url\": \"https://bobpay.xyz/pay\""
      "  }]"
      "}",
      "Bobpay", GURL("https://bobpay.xyz/bob/images/homescreen32.png"),
      GURL("https://bobpay.xyz/pay"));
}

// Tests that supplying no methods to the IOSPaymentInstrumentFinder class still
// invokes the caller's callback function and that the list of returned
// instruments is empty.
TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       NoMethodsSuppliedNoInstruments) {
  std::vector<GURL> url_methods;

  FindInstrumentsWithMethods(url_methods);

  EXPECT_EQ(0u, num_instruments_to_find());
  EXPECT_EQ(0u, result().size());
}

// Tests that supplying many invalid methods to the IOSPaymentInstrumentFinder
// class still invokes the caller's callback function and that the list of
// returned instruments is empty.
TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       ManyInvalidMethodsSuppliedNoInstruments) {
  test_url_loader_factory()->AddResponse("https://fake-host-name/bobpay",
                                         std::string(), net::HTTP_NOT_FOUND);
  test_url_loader_factory()->AddResponse("https://fake-host-name/alicepay",
                                         std::string(), net::HTTP_NOT_FOUND);
  test_url_loader_factory()->AddResponse("https://fake-host-name/sampay",
                                         std::string(), net::HTTP_NOT_FOUND);

  std::vector<GURL> url_methods;
  url_methods.push_back(GURL("https://fake-host-name/bobpay"));
  url_methods.push_back(GURL("https://fake-host-name/alicepay"));
  url_methods.push_back(GURL("https://fake-host-name/sampay"));

  FindInstrumentsWithMethods(url_methods);
  RunLoop();

  EXPECT_EQ(0u, num_instruments_to_find());
  EXPECT_EQ(0u, result().size());
}

// Tests that supplying one valid method with a corresponding complete web
// app manifest will result in one created IOSPaymentInstrument that is returned
// to the caller.
TEST_F(PaymentRequestIOSPaymentInstrumentFinderTest,
       OneValidMethodSuppliedOneInstrument) {
  test_url_loader_factory()->AddResponse(
      "https://bobpay.xyz/bob/images/homescreen32.png", /* content = */ "",
      net::HTTP_NOT_FOUND);
  FindInstrumentsWithWebAppManifest(
      GURL("https://emerald-eon.appspot.com/bobpay"),
      "{"
      "  \"short_name\": \"Bobpay\", "
      "  \"icons\": [{"
      "    \"src\": \"https://bobpay.xyz/bob/images/homescreen32.png\", "
      "    \"sizes\": \"32x32\""
      "  }], "
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"url\": \"https://bobpay.xyz/pay\""
      "  }, {"
      "    \"platform\": \"itunes\", "
      "    \"url\": \"https://bobpay.xyz/pay\""
      "  }]"
      "}");
  RunLoop();

  EXPECT_EQ(0u, num_instruments_to_find());
  EXPECT_EQ(1u, result().size());
  EXPECT_EQ("Bobpay", base::UTF16ToASCII(result()[0]->GetLabel()));
  EXPECT_EQ("emerald-eon.appspot.com",
            base::UTF16ToASCII(result()[0]->GetSublabel()));
}

}  // namespace payments
