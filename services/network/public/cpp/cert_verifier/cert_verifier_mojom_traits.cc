// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cert_verifier/cert_verifier_mojom_traits.h"

#include <string>
#include <vector>

#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"
#include "net/cert/x509_certificate.h"
#include "services/network/public/cpp/crash_keys.h"

namespace mojo {

bool StructTraits<cert_verifier::mojom::RequestParamsDataView,
                  net::CertVerifier::RequestParams>::
    Read(cert_verifier::mojom::RequestParamsDataView data,
         net::CertVerifier::RequestParams* params) {
  scoped_refptr<net::X509Certificate> certificate;
  std::string hostname, ocsp_response, sct_list;
  // TODO(https://crbug.com/1346598): Remove debug code once bug investigation
  // is complete.
  constexpr size_t kMaxAliasedStackDataSize = 8192;
  if (!data.ReadCertificate(&certificate)) {
    network::debug::SetDeserializationCrashKeyString("certificate");

    ::network::mojom::X509CertificateDataView cert_data_view;
    data.GetCertificateDataView(&cert_data_view);

    bool cert_data_view_is_null = cert_data_view.is_null();
    base::debug::Alias(&cert_data_view_is_null);
    if (cert_data_view_is_null) {
      base::debug::DumpWithoutCrashing();
      return false;
    }

    mojo::ArrayDataView<uint8_t> array_data_view;
    cert_data_view.GetDataDataView(&array_data_view);

    bool array_data_view_is_null = array_data_view.is_null();
    base::debug::Alias(&array_data_view_is_null);
    if (array_data_view_is_null) {
      base::debug::DumpWithoutCrashing();
      return false;
    }

    size_t size = array_data_view.size();
    uint8_t aliased_data[kMaxAliasedStackDataSize] = {0};
    size_t aliased_size = std::min(size, std::size(aliased_data));
    memcpy(aliased_data, array_data_view.data(), aliased_size);
    base::debug::Alias(&size);
    base::debug::Alias(&aliased_size);
    base::debug::Alias(aliased_data);

    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadHostname(&hostname)) {
    network::debug::SetDeserializationCrashKeyString("hostname");

    mojo::StringDataView string_data_view;
    data.GetHostnameDataView(&string_data_view);

    bool string_data_view_is_null = string_data_view.is_null();
    base::debug::Alias(&string_data_view_is_null);
    if (string_data_view_is_null) {
      base::debug::DumpWithoutCrashing();
      return false;
    }

    size_t size = string_data_view.size();
    uint8_t aliased_data[kMaxAliasedStackDataSize] = {0};
    size_t aliased_size = std::min(size, std::size(aliased_data));
    memcpy(aliased_data, string_data_view.storage(), aliased_size);
    base::debug::Alias(&size);
    base::debug::Alias(&aliased_size);
    base::debug::Alias(aliased_data);

    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadOcspResponse(&ocsp_response)) {
    network::debug::SetDeserializationCrashKeyString("ocsp_response");

    ::mojo_base::mojom::ByteStringDataView byte_string_data_view;
    data.GetOcspResponseDataView(&byte_string_data_view);

    bool byte_string_data_view_is_null = byte_string_data_view.is_null();
    base::debug::Alias(&byte_string_data_view_is_null);
    if (byte_string_data_view_is_null) {
      base::debug::DumpWithoutCrashing();
      return false;
    }

    mojo::ArrayDataView<uint8_t> array_data_view;
    byte_string_data_view.GetDataDataView(&array_data_view);

    bool array_data_view_is_null = array_data_view.is_null();
    base::debug::Alias(&array_data_view_is_null);
    if (array_data_view_is_null) {
      base::debug::DumpWithoutCrashing();
      return false;
    }

    size_t size = array_data_view.size();
    uint8_t aliased_data[kMaxAliasedStackDataSize] = {0};
    size_t aliased_size = std::min(size, std::size(aliased_data));
    memcpy(aliased_data, array_data_view.data(), aliased_size);
    base::debug::Alias(&size);
    base::debug::Alias(&aliased_size);
    base::debug::Alias(aliased_data);

    base::debug::DumpWithoutCrashing();
    return false;
  }
  if (!data.ReadSctList(&sct_list)) {
    network::debug::SetDeserializationCrashKeyString("sct_list");

    ::mojo_base::mojom::ByteStringDataView byte_string_data_view;
    data.GetSctListDataView(&byte_string_data_view);

    bool byte_string_data_view_is_null = byte_string_data_view.is_null();
    base::debug::Alias(&byte_string_data_view_is_null);
    if (byte_string_data_view_is_null) {
      base::debug::DumpWithoutCrashing();
      return false;
    }

    mojo::ArrayDataView<uint8_t> array_data_view;
    byte_string_data_view.GetDataDataView(&array_data_view);

    bool array_data_view_is_null = array_data_view.is_null();
    base::debug::Alias(&array_data_view_is_null);
    if (array_data_view_is_null) {
      base::debug::DumpWithoutCrashing();
      return false;
    }

    size_t size = array_data_view.size();
    uint8_t aliased_data[kMaxAliasedStackDataSize] = {0};
    size_t aliased_size = std::min(size, std::size(aliased_data));
    memcpy(aliased_data, array_data_view.data(), aliased_size);
    base::debug::Alias(&size);
    base::debug::Alias(&aliased_size);
    base::debug::Alias(aliased_data);

    base::debug::DumpWithoutCrashing();
    return false;
  }
  *params = net::CertVerifier::RequestParams(
      std::move(certificate), std::move(hostname), data.flags(),
      std::move(ocsp_response), std::move(sct_list));
  return true;
}

bool StructTraits<cert_verifier::mojom::CertVerifierConfigDataView,
                  net::CertVerifier::Config>::
    Read(cert_verifier::mojom::CertVerifierConfigDataView data,
         net::CertVerifier::Config* config) {
  mojo_base::BigBuffer crl_set_buffer;
  std::vector<scoped_refptr<net::X509Certificate>> additional_trust_anchors;
  std::vector<scoped_refptr<net::X509Certificate>>
      additional_untrusted_authorities;
  if (!data.ReadCrlSet(&crl_set_buffer) ||
      !data.ReadAdditionalTrustAnchors(&additional_trust_anchors) ||
      !data.ReadAdditionalUntrustedAuthorities(
          &additional_untrusted_authorities))
    return false;

  scoped_refptr<net::CRLSet> crl_set;
  if (crl_set_buffer.size() != 0) {
    // Make a copy from shared memory so we can avoid double-fetch bugs.
    std::string crl_set_string(
        reinterpret_cast<const char*>(crl_set_buffer.data()),
        crl_set_buffer.size());
    net::CRLSet::ParseAndStoreUnparsedData(crl_set_string, &crl_set);
  }

  config->enable_rev_checking = data.enable_rev_checking();
  config->require_rev_checking_local_anchors =
      data.require_rev_checking_local_anchors();
  config->enable_sha1_local_anchors = data.enable_sha1_local_anchors();
  config->disable_symantec_enforcement = data.disable_symantec_enforcement();
  config->crl_set = std::move(crl_set);
  config->additional_trust_anchors = std::move(additional_trust_anchors);
  config->additional_untrusted_authorities =
      std::move(additional_untrusted_authorities);
  return true;
}

}  // namespace mojo
