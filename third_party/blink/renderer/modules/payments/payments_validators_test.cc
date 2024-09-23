// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/payments/payments_validators.h"

#include <ostream>

#include "base/test/scoped_command_line.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_address_errors.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payer_errors.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_validation_errors.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

struct CurrencyCodeTestCase {
  CurrencyCodeTestCase(const char* code, bool expected_valid)
      : code(code), expected_valid(expected_valid) {}
  ~CurrencyCodeTestCase() = default;

  const char* code;
  bool expected_valid;
};

class PaymentsCurrencyValidatorTest
    : public testing::TestWithParam<CurrencyCodeTestCase> {
 public:
  v8::Isolate* GetIsolate() { return task_environment_.isolate(); }

  test::TaskEnvironment task_environment_;
};

const char* LongString2049() {
  static char long_string[2050];
  for (int i = 0; i < 2049; i++)
    long_string[i] = 'a';
  long_string[2049] = '\0';
  return long_string;
}

TEST_P(PaymentsCurrencyValidatorTest, IsValidCurrencyCodeFormat) {
  String error_message;
  EXPECT_EQ(GetParam().expected_valid,
            PaymentsValidators::IsValidCurrencyCodeFormat(
                GetIsolate(), GetParam().code, &error_message))
      << error_message;
  EXPECT_EQ(GetParam().expected_valid, error_message.empty()) << error_message;

  EXPECT_EQ(GetParam().expected_valid,
            PaymentsValidators::IsValidCurrencyCodeFormat(
                GetIsolate(), GetParam().code, nullptr));
}

INSTANTIATE_TEST_SUITE_P(
    CurrencyCodes,
    PaymentsCurrencyValidatorTest,
    testing::Values(
        // The most common identifiers are three-letter alphabetic codes as
        // defined by [ISO4217] (for example, "USD" for US Dollars).
        // |system| is a URL that indicates the currency system that the
        // currency identifier belongs to. By default,
        // the value is urn:iso:std:iso:4217 indicating that currency is defined
        // by [[ISO4217]], however any string of at most 2048
        // characters is considered valid in other currencySystem. Returns false
        // if currency |code| is too long (greater than 2048).
        CurrencyCodeTestCase("USD", true),
        CurrencyCodeTestCase("US1", false),
        CurrencyCodeTestCase("US", false),
        CurrencyCodeTestCase("USDO", false),
        CurrencyCodeTestCase("usd", true),
        CurrencyCodeTestCase("ANYSTRING", false),
        CurrencyCodeTestCase("", false),
        CurrencyCodeTestCase(LongString2049(), false)));

struct TestCase {
  TestCase(const char* input, bool expected_valid)
      : input(input), expected_valid(expected_valid) {}
  ~TestCase() = default;

  const char* input;
  bool expected_valid;
};

std::ostream& operator<<(std::ostream& out, const TestCase& test_case) {
  out << "'" << test_case.input << "' is expected to be "
      << (test_case.expected_valid ? "valid" : "invalid");
  return out;
}

class PaymentsAmountValidatorTest : public testing::TestWithParam<TestCase> {
 public:
  v8::Isolate* GetIsolate() { return task_environment_.isolate(); }
  test::TaskEnvironment task_environment_;
};

TEST_P(PaymentsAmountValidatorTest, IsValidAmountFormat) {
  String error_message;
  EXPECT_EQ(GetParam().expected_valid,
            PaymentsValidators::IsValidAmountFormat(
                GetIsolate(), GetParam().input, "test value", &error_message))
      << error_message;
  EXPECT_EQ(GetParam().expected_valid, error_message.empty()) << error_message;

  EXPECT_EQ(GetParam().expected_valid,
            PaymentsValidators::IsValidAmountFormat(
                GetIsolate(), GetParam().input, "test value", nullptr));
}

INSTANTIATE_TEST_SUITE_P(
    Amounts,
    PaymentsAmountValidatorTest,
    testing::Values(TestCase("0", true),
                    TestCase("-0", true),
                    TestCase("1", true),
                    TestCase("10", true),
                    TestCase("-3", true),
                    TestCase("10.99", true),
                    TestCase("-3.00", true),
                    TestCase("01234567890123456789.0123456789", true),
                    TestCase("01234567890123456789012345678.9", true),
                    TestCase("012345678901234567890123456789", true),
                    TestCase("-01234567890123456789.0123456789", true),
                    TestCase("-01234567890123456789012345678.9", true),
                    TestCase("-012345678901234567890123456789", true),
                    // Invalid amount formats
                    TestCase("", false),
                    TestCase("-", false),
                    TestCase("notdigits", false),
                    TestCase("ALSONOTDIGITS", false),
                    TestCase("10.", false),
                    TestCase(".99", false),
                    TestCase("-10.", false),
                    TestCase("-.99", false),
                    TestCase("10-", false),
                    TestCase("1-0", false),
                    TestCase("1.0.0", false),
                    TestCase("1/3", false)));

class PaymentsRegionValidatorTest : public testing::TestWithParam<TestCase> {
 public:
  v8::Isolate* GetIsolate() { return task_environment_.isolate(); }
  test::TaskEnvironment task_environment_;
};

TEST_P(PaymentsRegionValidatorTest, IsValidCountryCodeFormat) {
  String error_message;
  EXPECT_EQ(GetParam().expected_valid,
            PaymentsValidators::IsValidCountryCodeFormat(
                GetIsolate(), GetParam().input, &error_message))
      << error_message;
  EXPECT_EQ(GetParam().expected_valid, error_message.empty()) << error_message;

  EXPECT_EQ(GetParam().expected_valid,
            PaymentsValidators::IsValidCountryCodeFormat(
                GetIsolate(), GetParam().input, nullptr));
}

INSTANTIATE_TEST_SUITE_P(CountryCodes,
                         PaymentsRegionValidatorTest,
                         testing::Values(TestCase("US", true),
                                         // Invalid country code formats
                                         TestCase("U1", false),
                                         TestCase("U", false),
                                         TestCase("us", false),
                                         TestCase("USA", false),
                                         TestCase("", false)));

struct ShippingAddressTestCase {
  ShippingAddressTestCase(const char* country_code, bool expected_valid)
      : country_code(country_code), expected_valid(expected_valid) {}
  ~ShippingAddressTestCase() = default;

  const char* country_code;
  bool expected_valid;
};

class PaymentsShippingAddressValidatorTest
    : public testing::TestWithParam<ShippingAddressTestCase> {
 public:
  v8::Isolate* GetIsolate() { return task_environment_.isolate(); }

  test::TaskEnvironment task_environment_;
};

TEST_P(PaymentsShippingAddressValidatorTest, IsValidShippingAddress) {
  payments::mojom::blink::PaymentAddressPtr address =
      payments::mojom::blink::PaymentAddress::New();
  address->country = GetParam().country_code;

  String error_message;
  EXPECT_EQ(GetParam().expected_valid,
            PaymentsValidators::IsValidShippingAddress(GetIsolate(), address,
                                                       &error_message))
      << error_message;
  EXPECT_EQ(GetParam().expected_valid, error_message.empty()) << error_message;

  EXPECT_EQ(GetParam().expected_valid,
            PaymentsValidators::IsValidShippingAddress(GetIsolate(), address,
                                                       nullptr));
}

INSTANTIATE_TEST_SUITE_P(
    ShippingAddresses,
    PaymentsShippingAddressValidatorTest,
    testing::Values(ShippingAddressTestCase("US", true),
                    ShippingAddressTestCase("US", true),
                    ShippingAddressTestCase("US", true),
                    // Invalid shipping addresses
                    ShippingAddressTestCase("", false),
                    ShippingAddressTestCase("InvalidCountryCode", false)));

struct ValidationErrorsTestCase {
  ValidationErrorsTestCase(bool expected_valid)
      : expected_valid(expected_valid) {}

  const char* m_error = "";
  const char* m_payer_email = "";
  const char* m_payer_name = "";
  const char* m_payer_phone = "";
  const char* m_shipping_address_address_line = "";
  const char* m_shipping_address_city = "";
  const char* m_shipping_address_country = "";
  const char* m_shipping_address_dependent_locality = "";
  const char* m_shipping_address_organization = "";
  const char* m_shipping_address_phone = "";
  const char* m_shipping_address_postal_code = "";
  const char* m_shipping_address_recipient = "";
  const char* m_shipping_address_region = "";
  const char* m_shipping_address_sorting_code = "";
  bool expected_valid;
};

#define VALIDATION_ERRORS_TEST_CASE(field, value, expected_valid) \
  ([]() {                                                         \
    ValidationErrorsTestCase test_case(expected_valid);           \
    test_case.m_##field = value;                                  \
    return test_case;                                             \
  })()

PaymentValidationErrors* toPaymentValidationErrors(
    ValidationErrorsTestCase test_case) {
  PaymentValidationErrors* errors = PaymentValidationErrors::Create();

  PayerErrors* payer = PayerErrors::Create();
  payer->setEmail(test_case.m_payer_email);
  payer->setName(test_case.m_payer_name);
  payer->setPhone(test_case.m_payer_phone);

  AddressErrors* shipping_address = AddressErrors::Create();
  shipping_address->setAddressLine(test_case.m_shipping_address_address_line);
  shipping_address->setCity(test_case.m_shipping_address_city);
  shipping_address->setCountry(test_case.m_shipping_address_country);
  shipping_address->setDependentLocality(
      test_case.m_shipping_address_dependent_locality);
  shipping_address->setOrganization(test_case.m_shipping_address_organization);
  shipping_address->setPhone(test_case.m_shipping_address_phone);
  shipping_address->setPostalCode(test_case.m_shipping_address_postal_code);
  shipping_address->setRecipient(test_case.m_shipping_address_recipient);
  shipping_address->setRegion(test_case.m_shipping_address_region);
  shipping_address->setSortingCode(test_case.m_shipping_address_sorting_code);

  errors->setError(test_case.m_error);
  errors->setPayer(payer);
  errors->setShippingAddress(shipping_address);

  return errors;
}

class PaymentsErrorMessageValidatorTest
    : public testing::TestWithParam<ValidationErrorsTestCase> {};

TEST_P(PaymentsErrorMessageValidatorTest,
       IsValidPaymentValidationErrorsFormat) {
  PaymentValidationErrors* errors = toPaymentValidationErrors(GetParam());

  String error_message;
  EXPECT_EQ(GetParam().expected_valid,
            PaymentsValidators::IsValidPaymentValidationErrorsFormat(
                errors, &error_message))
      << error_message;
}

INSTANTIATE_TEST_SUITE_P(
    PaymentValidationErrorss,
    PaymentsErrorMessageValidatorTest,
    testing::Values(
        VALIDATION_ERRORS_TEST_CASE(error, "test", true),
        VALIDATION_ERRORS_TEST_CASE(payer_email, "test", true),
        VALIDATION_ERRORS_TEST_CASE(payer_name, "test", true),
        VALIDATION_ERRORS_TEST_CASE(payer_phone, "test", true),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_city, "test", true),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_address_line,
                                    "test",
                                    true),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_city, "test", true),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_country, "test", true),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_dependent_locality,
                                    "test",
                                    true),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_organization,
                                    "test",
                                    true),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_phone, "test", true),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_postal_code, "test", true),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_recipient, "test", true),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_region, "test", true),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_sorting_code,
                                    "test",
                                    true),
        VALIDATION_ERRORS_TEST_CASE(error, LongString2049(), false),
        VALIDATION_ERRORS_TEST_CASE(payer_email, LongString2049(), false),
        VALIDATION_ERRORS_TEST_CASE(payer_name, LongString2049(), false),
        VALIDATION_ERRORS_TEST_CASE(payer_phone, LongString2049(), false),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_city,
                                    LongString2049(),
                                    false),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_address_line,
                                    LongString2049(),
                                    false),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_city,
                                    LongString2049(),
                                    false),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_country,
                                    LongString2049(),
                                    false),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_dependent_locality,
                                    LongString2049(),
                                    false),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_organization,
                                    LongString2049(),
                                    false),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_phone,
                                    LongString2049(),
                                    false),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_postal_code,
                                    LongString2049(),
                                    false),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_recipient,
                                    LongString2049(),
                                    false),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_region,
                                    LongString2049(),
                                    false),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_sorting_code,
                                    LongString2049(),
                                    false)));

class PaymentMethodValidatorTest : public testing::Test {
 public:
  v8::Isolate* GetIsolate() { return task_environment_.isolate(); }
  test::TaskEnvironment task_environment_;
};

TEST_F(PaymentMethodValidatorTest, IsValidPaymentMethod) {
  const struct {
    const char* payment_method;
    bool expected_valid;
  } kTestCases[] = {{"basic-card", true},
                    {"https://bobpay.com", true},
                    {"https://pay.bobpay.com", true},
                    {"https://pay.bobpay.com/pay", true},
                    {"https://pay.bobpay.com/pay?version=1", true},
                    {"https://pay.bobpay.com/pay#", true},
                    {"http://localhost", true},
                    {"http://localhost:8080", true},
                    {"http://bobpay.com", false},
                    {"https://username:password@bobpay.com", false},
                    {"https://username@bobpay.com", false},
                    {"unknown://bobpay.com", false},
                    {"1card", false},
                    {"Basic-card", false}};

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected_valid,
              PaymentsValidators::IsValidMethodFormat(GetIsolate(),
                                                      test_case.payment_method))
        << test_case.payment_method << " should be "
        << (test_case.expected_valid ? "valid" : "invalid");
  }
}

TEST_F(PaymentMethodValidatorTest, IsValidPaymentMethodSafelisted) {
  EXPECT_FALSE(PaymentsValidators::IsValidMethodFormat(GetIsolate(),
                                                       "http://alicepay.com"))
      << "http://alicepay.com is not a valid method format by default";

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(
      network::switches::kUnsafelyTreatInsecureOriginAsSecure,
      "http://alicepay.com");
  network::SecureOriginAllowlist::GetInstance().ResetForTesting();

  EXPECT_TRUE(PaymentsValidators::IsValidMethodFormat(GetIsolate(),
                                                      "http://alicepay.com"))
      << "http://alicepay.com should be valid if safelisted";
}

}  // namespace
}  // namespace blink
