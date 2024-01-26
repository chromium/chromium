// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_signed_certificate_timestamp_log_param.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "net/cert/ct_sct_to_string.h"
#include "net/cert/signed_certificate_timestamp.h"

namespace net {

namespace {

// Base64 encode the given |value| string and put it in |dict| with the
// description |key|.
void SetBinaryData(const char* key,
                   std::string_view value,
                   base::Value::Dict& dict) {
  std::string b64_value = base::Base64Encode(value);

  dict.Set(key, b64_value);
}

// Returns a dictionary where each key is a field of the SCT and its value
// is this field's value in the SCT. This dictionary is meant to be used for
// outputting a de-serialized SCT to the NetLog.
base::Value SCTToDictionary(const ct::SignedCertificateTimestamp& sct,
                            ct::SCTVerifyStatus status) {
  base::Value::Dict dict;

  dict.Set("origin", OriginToString(sct.origin));
  dict.Set("verification_status", StatusToString(status));
  dict.Set("version", sct.version);

  SetBinaryData("log_id", sct.log_id, dict);
  base::TimeDelta time_since_unix_epoch =
      sct.timestamp - base::Time::UnixEpoch();
  dict.Set("timestamp",
           base::NumberToString(time_since_unix_epoch.InMilliseconds()));
  SetBinaryData("extensions", sct.extensions, dict);

  dict.Set("hash_algorithm",
           HashAlgorithmToString(sct.signature.hash_algorithm));
  dict.Set("signature_algorithm",
           SignatureAlgorithmToString(sct.signature.signature_algorithm));
  SetBinaryData("signature_data", sct.signature.signature_data, dict);

  return base::Value(std::move(dict));
}

// Given a list of SCTs and their statuses, return a list Value where each item
// is a dictionary created by SCTToDictionary.
base::Value::List SCTListToPrintableValues(
    const SignedCertificateTimestampAndStatusList& sct_and_status_list) {
  base::Value::List output_scts;
  for (const auto& sct_and_status : sct_and_status_list) {
    output_scts.Append(
        SCTToDictionary(*(sct_and_status.sct.get()), sct_and_status.status));
  }

  return output_scts;
}

}  // namespace

base::Value::Dict NetLogSignedCertificateTimestampParams(
    const SignedCertificateTimestampAndStatusList* scts) {
  base::Value::Dict dict;

  dict.Set("scts", SCTListToPrintableValues(*scts));

  return dict;
}

base::Value::Dict NetLogRawSignedCertificateTimestampParams(
    std::string_view embedded_scts,
    std::string_view sct_list_from_ocsp,
    std::string_view sct_list_from_tls_extension) {
  base::Value::Dict dict;

  SetBinaryData("embedded_scts", embedded_scts, dict);
  SetBinaryData("scts_from_ocsp_response", sct_list_from_ocsp, dict);
  SetBinaryData("scts_from_tls_extension", sct_list_from_tls_extension, dict);

  return dict;
}

}  // namespace net
