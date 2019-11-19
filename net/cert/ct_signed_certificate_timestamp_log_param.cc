// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_signed_certificate_timestamp_log_param.h"

#include <algorithm>
#include <memory>
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
                   base::StringPiece value,
                   base::Value* dict) {
  std::string b64_value;
  base::Base64Encode(value, &b64_value);

  dict->SetStringKey(key, b64_value);
}

// Returns a dictionary where each key is a field of the SCT and its value
// is this field's value in the SCT. This dictionary is meant to be used for
// outputting a de-serialized SCT to the NetLog.
base::Value SCTToDictionary(const ct::SignedCertificateTimestamp& sct,
                            ct::SCTVerifyStatus status) {
  base::Value out(base::Value::Type::DICTIONARY);

  out.SetStringKey("origin", OriginToString(sct.origin));
  out.SetStringKey("verification_status", StatusToString(status));
  out.SetIntKey("version", sct.version);

  SetBinaryData("log_id", sct.log_id, &out);
  base::TimeDelta time_since_unix_epoch =
      sct.timestamp - base::Time::UnixEpoch();
  out.SetStringKey("timestamp", base::NumberToString(
                                    time_since_unix_epoch.InMilliseconds()));
  SetBinaryData("extensions", sct.extensions, &out);

  out.SetStringKey("hash_algorithm",
                   HashAlgorithmToString(sct.signature.hash_algorithm));
  out.SetStringKey(
      "signature_algorithm",
      SignatureAlgorithmToString(sct.signature.signature_algorithm));
  SetBinaryData("signature_data", sct.signature.signature_data, &out);

  return out;
}

// Given a list of SCTs and their statuses, return a list Value where each item
// is a dictionary created by SCTToDictionary.
base::Value SCTListToPrintableValues(
    const SignedCertificateTimestampAndStatusList& sct_and_status_list) {
  base::Value output_scts(base::Value::Type::LIST);
  for (const auto& sct_and_status : sct_and_status_list)
    output_scts.Append(
        SCTToDictionary(*(sct_and_status.sct.get()), sct_and_status.status));

  return output_scts;
}

}  // namespace

base::Value NetLogSignedCertificateTimestampParams(
    const SignedCertificateTimestampAndStatusList* scts) {
  base::Value dict(base::Value::Type::DICTIONARY);

  dict.SetKey("scts", SCTListToPrintableValues(*scts));

  return dict;
}

base::Value NetLogRawSignedCertificateTimestampParams(
    base::StringPiece embedded_scts,
    base::StringPiece sct_list_from_ocsp,
    base::StringPiece sct_list_from_tls_extension) {
  base::Value dict(base::Value::Type::DICTIONARY);

  SetBinaryData("embedded_scts", embedded_scts, &dict);
  SetBinaryData("scts_from_ocsp_response", sct_list_from_ocsp, &dict);
  SetBinaryData("scts_from_tls_extension", sct_list_from_tls_extension, &dict);

  return dict;
}

}  // namespace net
