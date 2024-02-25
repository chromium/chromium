/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2009 Michelangelo De Simone <micdesim@gmail.com>
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/html/forms/email_input_type.h"

#include <unicode/idna.h>
#include <unicode/unistr.h>
#include <unicode/uvernum.h>
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/platform/bindings/script_regexp.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

#if U_ICU_VERSION_MAJOR_NUM >= 59
#include <unicode/char16ptr.h>
#endif

namespace {

// http://www.whatwg.org/specs/web-apps/current-work/multipage/states-of-the-type-attribute.html#valid-e-mail-address
const char kLocalPartCharacters[] =
    "abcdefghijklmnopqrstuvwxyz0123456789!#$%&'*+/=?^_`{|}~.-";
const char kEmailPattern[] =
    "[a-z0-9!#$%&'*+/=?^_`{|}~.-]+"  // local part
    "@"
    "[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?"  // domain part
    "(?:\\.[a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?)*";

// RFC5321 says the maximum total length of a domain name is 255 octets.
const int32_t kMaximumDomainNameLength = 255;

// Use the same option as in url/url_canon_icu.cc
// TODO(crbug.com/694157): Change the options if UseIDNA2008NonTransitional flag
// is enabled.
const int32_t kIdnaConversionOption = UIDNA_CHECK_BIDI;

}  // namespace

namespace blink {

ScriptRegexp* EmailInputType::CreateEmailRegexp(v8::Isolate* isolate) {
  return MakeGarbageCollected<ScriptRegexp>(isolate, kEmailPattern,
                                            kTextCaseUnicodeInsensitive);
}

Vector<String> EmailInputType::ParseMultipleValues(const String& value) {
  Vector<String> values;
  value.Split(',', true, values);
  return values;
}

String EmailInputType::ConvertEmailAddressToASCII(const ScriptRegexp& regexp,
                                                  const String& address) {
  if (address.ContainsOnlyASCIIOrEmpty())
    return address;

  wtf_size_t at_position = address.find('@');
  if (at_position == kNotFound)
    return address;
  String host = address.Substring(at_position + 1);

  // UnicodeString ctor for copy-on-write does not work reliably (in debug
  // build.) TODO(jshin): In an unlikely case this is a perf-issue, treat
  // 8bit and non-8bit strings separately.
  host.Ensure16Bit();
  icu::UnicodeString idn_domain_name(host.Characters16(), host.length());
  icu::UnicodeString domain_name;

  // Leak |idna| at the end.
  UErrorCode error_code = U_ZERO_ERROR;
  static const icu::IDNA* const idna =
      icu::IDNA::createUTS46Instance(kIdnaConversionOption, error_code);
  DCHECK(idna);
  icu::IDNAInfo idna_info;
  idna->nameToASCII(idn_domain_name, domain_name, idna_info, error_code);
  if (U_FAILURE(error_code) || idna_info.hasErrors() ||
      domain_name.length() > kMaximumDomainNameLength)
    return address;

  StringBuilder builder;
  builder.Append(address, 0, at_position + 1);
#if U_ICU_VERSION_MAJOR_NUM >= 59
  builder.Append(icu::toUCharPtr(domain_name.getBuffer()), domain_name.length());
#else
  builder.Append(domain_name.getBuffer(), domain_name.length());
#endif
  String ascii_email = builder.ToString();
  return IsValidEmailAddress(regexp, ascii_email) ? ascii_email : address;
}

String EmailInputType::ConvertEmailAddressToUnicode(
    const String& address) const {
  if (!address.ContainsOnlyASCIIOrEmpty())
    return address;

  wtf_size_t at_position = address.find('@');
  if (at_position == kNotFound)
    return address;

  if (address.Find("xn--", at_position + 1) == kNotFound)
    return address;

  String unicode_host = Platform::Current()->ConvertIDNToUnicode(
      address.Substring(at_position + 1));
  StringBuilder builder;
  builder.Append(address, 0, at_position + 1);
  builder.Append(unicode_host);
  return builder.ToString();
}

static bool IsInvalidLocalPartCharacter(UChar ch) {
  if (!IsASCII(ch))
    return true;
  DEFINE_STATIC_LOCAL(const String, valid_characters, (kLocalPartCharacters));
  return valid_characters.find(ToASCIILower(ch)) == kNotFound;
}

static bool IsInvalidDomainCharacter(UChar ch) {
  if (!IsASCII(ch))
    return true;
  return !IsASCIILower(ch) && !IsASCIIUpper(ch) && !IsASCIIDigit(ch) &&
         ch != '.' && ch != '-';
}

static bool CheckValidDotUsage(const String& domain) {
  if (domain.empty())
    return true;
  if (domain[0] == '.' || domain[domain.length() - 1] == '.')
    return false;
  return domain.Find("..") == kNotFound;
}

bool EmailInputType::IsValidEmailAddress(const ScriptRegexp& regexp,
                                         const String& address) {
  int address_length = address.length();
  if (!address_length)
    return false;

  int match_length;
  int match_offset = regexp.Match(address, 0, &match_length);

  return !match_offset && match_length == address_length;
}

EmailInputType::EmailInputType(HTMLInputElement& element)
    : BaseTextInputType(Type::kEmail, element) {}

void EmailInputType::CountUsage() {
  CountUsageIfVisible(WebFeature::kInputTypeEmail);
  bool has_max_length =
      GetElement().FastHasAttribute(html_names::kMaxlengthAttr);
  if (has_max_length)
    CountUsageIfVisible(WebFeature::kInputTypeEmailMaxLength);
  if (GetElement().Multiple()) {
    CountUsageIfVisible(WebFeature::kInputTypeEmailMultiple);
    if (has_max_length)
      CountUsageIfVisible(WebFeature::kInputTypeEmailMultipleMaxLength);
  }
}

// The return value is an invalid email address string if the specified string
// contains an invalid email address. Otherwise, an empty string is returned.
// If an empty string is returned, it means empty address is specified.
// e.g. "foo@example.com,,bar@example.com" for multiple case.
String EmailInputType::FindInvalidAddress(const String& value) const {
  if (value.empty())
    return String();
  if (!GetElement().Multiple()) {
    return IsValidEmailAddress(GetElement().GetDocument().EnsureEmailRegexp(),
                               value)
               ? String()
               : value;
  }
  Vector<String> addresses = ParseMultipleValues(value);
  for (const auto& address : addresses) {
    String stripped = StripLeadingAndTrailingHTMLSpaces(address);
    if (!IsValidEmailAddress(GetElement().GetDocument().EnsureEmailRegexp(),
                             stripped))
      return stripped;
  }
  return String();
}

bool EmailInputType::TypeMismatchFor(const String& value) const {
  return !FindInvalidAddress(value).IsNull();
}

bool EmailInputType::TypeMismatch() const {
  return TypeMismatchFor(GetElement().Value());
}

String EmailInputType::TypeMismatchText() const {
  String invalid_address = FindInvalidAddress(GetElement().Value());
  DCHECK(!invalid_address.IsNull());
  if (invalid_address.empty()) {
    return GetLocale().QueryString(
        IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_EMPTY);
  }
  String at_sign = String("@");
  wtf_size_t at_index = invalid_address.find('@');
  if (at_index == kNotFound)
    return GetLocale().QueryString(
        IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_NO_AT_SIGN, at_sign,
        invalid_address);
  // We check validity against an ASCII value because of difficulty to check
  // invalid characters. However we should show Unicode value.
  String unicode_address = ConvertEmailAddressToUnicode(invalid_address);
  String local_part = invalid_address.Left(at_index);
  String domain = invalid_address.Substring(at_index + 1);
  if (local_part.empty())
    return GetLocale().QueryString(
        IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_EMPTY_LOCAL, at_sign,
        unicode_address);
  if (domain.empty())
    return GetLocale().QueryString(
        IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_EMPTY_DOMAIN, at_sign,
        unicode_address);
  wtf_size_t invalid_char_index = local_part.Find(IsInvalidLocalPartCharacter);
  if (invalid_char_index != kNotFound) {
    unsigned char_length = U_IS_LEAD(local_part[invalid_char_index]) ? 2 : 1;
    return GetLocale().QueryString(
        IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_INVALID_LOCAL, at_sign,
        local_part.Substring(invalid_char_index, char_length));
  }
  invalid_char_index = domain.Find(IsInvalidDomainCharacter);
  if (invalid_char_index != kNotFound) {
    unsigned char_length = U_IS_LEAD(domain[invalid_char_index]) ? 2 : 1;
    return GetLocale().QueryString(
        IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_INVALID_DOMAIN, at_sign,
        domain.Substring(invalid_char_index, char_length));
  }
  if (!CheckValidDotUsage(domain)) {
    wtf_size_t at_index_in_unicode = unicode_address.find('@');
    DCHECK_NE(at_index_in_unicode, kNotFound);
    return GetLocale().QueryString(
        IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_INVALID_DOTS, String("."),
        unicode_address.Substring(at_index_in_unicode + 1));
  }
  if (GetElement().Multiple()) {
    return GetLocale().QueryString(
        IDS_FORM_VALIDATION_TYPE_MISMATCH_MULTIPLE_EMAIL);
  }
  return GetLocale().QueryString(IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL);
}

bool EmailInputType::SupportsSelectionAPI() const {
  return false;
}

String EmailInputType::SanitizeValue(const String& proposed_value) const {
  String no_line_break_value = proposed_value.RemoveCharacters(IsHTMLLineBreak);
  if (!GetElement().Multiple())
    return StripLeadingAndTrailingHTMLSpaces(no_line_break_value);
  Vector<String> addresses = ParseMultipleValues(no_line_break_value);
  StringBuilder stripped_value;
  for (wtf_size_t i = 0; i < addresses.size(); ++i) {
    if (i > 0)
      stripped_value.Append(',');
    stripped_value.Append(StripLeadingAndTrailingHTMLSpaces(addresses[i]));
  }
  return stripped_value.ToString();
}

String EmailInputType::ConvertFromVisibleValue(
    const String& visible_value) const {
  String sanitized_value = SanitizeValue(visible_value);
  if (!GetElement().Multiple()) {
    return ConvertEmailAddressToASCII(
        GetElement().GetDocument().EnsureEmailRegexp(), sanitized_value);
  }
  Vector<String> addresses = ParseMultipleValues(sanitized_value);
  StringBuilder builder;
  builder.ReserveCapacity(sanitized_value.length());
  for (wtf_size_t i = 0; i < addresses.size(); ++i) {
    if (i > 0)
      builder.Append(',');
    builder.Append(ConvertEmailAddressToASCII(
        GetElement().GetDocument().EnsureEmailRegexp(), addresses[i]));
  }
  return builder.ToString();
}

String EmailInputType::VisibleValue() const {
  String value = GetElement().Value();
  if (!GetElement().Multiple())
    return ConvertEmailAddressToUnicode(value);

  Vector<String> addresses = ParseMultipleValues(value);
  StringBuilder builder;
  builder.ReserveCapacity(value.length());
  for (wtf_size_t i = 0; i < addresses.size(); ++i) {
    if (i > 0)
      builder.Append(',');
    builder.Append(ConvertEmailAddressToUnicode(addresses[i]));
  }
  return builder.ToString();
}

void EmailInputType::MultipleAttributeChanged() {
  GetElement().SetValueFromRenderer(SanitizeValue(GetElement().Value()));
}

}  // namespace blink
