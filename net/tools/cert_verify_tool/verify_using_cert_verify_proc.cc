// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/cert_verify_tool/verify_using_cert_verify_proc.h"

#include <algorithm>
#include <iostream>
#include <string_view>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/log/net_log_with_source.h"
#include "net/tools/cert_verify_tool/cert_verify_tool_util.h"

namespace {

// Associates a printable name with an integer constant. Useful for providing
// human-readable decoding of bitmask values.
struct StringToConstant {
  const char* name;
  const int constant;
};

const StringToConstant kCertStatusFlags[] = {
#define CERT_STATUS_FLAG(label, value) {#label, value},
#include "net/cert/cert_status_flags_list.h"
#undef CERT_STATUS_FLAG
};

// Writes a PEM-encoded file of |cert| and its chain.
bool DumpX509CertificateChain(const base::FilePath& file_path,
                              const net::X509Certificate* cert) {
  std::vector<std::string> pem_encoded;
  if (!cert->GetPEMEncodedChain(&pem_encoded)) {
    std::cerr << "ERROR: X509Certificate::GetPEMEncodedChain failed.\n";
    return false;
  }
  return WriteToFile(file_path, base::StrCat(pem_encoded));
}

void PrintCertStatus(int cert_status) {
  std::cout << base::StringPrintf("CertStatus: 0x%x\n", cert_status);

  for (const auto& flag : kCertStatusFlags) {
    if ((cert_status & flag.constant) == flag.constant)
      std::cout << " " << flag.name << "\n";
  }
}

}  // namespace

void PrintCertVerifyResult(const net::CertVerifyResult& result) {
  PrintCertStatus(result.cert_status);
  if (result.has_sha1)
    std::cout << "has_sha1\n";
  if (result.is_issued_by_known_root)
    std::cout << "is_issued_by_known_root\n";
  if (result.is_issued_by_additional_trust_anchor)
    std::cout << "is_issued_by_additional_trust_anchor\n";

  if (result.verified_cert) {
    std::cout << "chain:\n "
              << FingerPrintCryptoBuffer(result.verified_cert->cert_buffer())
              << " " << SubjectFromX509Certificate(result.verified_cert.get())
              << "\n";
    for (const auto& intermediate :
         result.verified_cert->intermediate_buffers()) {
      std::cout << " " << FingerPrintCryptoBuffer(intermediate.get()) << " "
                << SubjectFromCryptoBuffer(intermediate.get()) << "\n";
    }
  }
}

bool VerifyUsingCertVerifyProc(
    net::CertVerifyProc* cert_verify_proc,
    const CertInput& target_der_cert,
    const std::string& hostname,
    const std::vector<CertInput>& intermediate_der_certs,
    const std::vector<CertInputWithTrustSetting>& der_certs_with_trust_settings,
    const base::FilePath& dump_path) {
  std::vector<std::string_view> der_cert_chain;
  der_cert_chain.push_back(target_der_cert.der_cert);
  for (const auto& cert : intermediate_der_certs)
    der_cert_chain.push_back(cert.der_cert);

  scoped_refptr<net::X509Certificate> x509_target_and_intermediates =
      net::X509Certificate::CreateFromDERCertChain(der_cert_chain);
  if (!x509_target_and_intermediates) {
    std::cerr
        << "ERROR: X509Certificate::CreateFromDERCertChain failed on one or "
           "more of:\n";
    PrintCertError(" (target)", target_der_cert);
    for (const auto& cert : intermediate_der_certs)
      PrintCertError(" (intermediate)", cert);
    return false;
  }

  net::TestRootCerts* test_root_certs = net::TestRootCerts::GetInstance();
  CHECK(test_root_certs->IsEmpty());

  std::vector<net::ScopedTestRoot> scoped_test_roots;
  for (const auto& cert_input_with_trust : der_certs_with_trust_settings) {
    scoped_refptr<net::X509Certificate> x509_root =
        net::X509Certificate::CreateFromBytes(base::as_bytes(
            base::make_span(cert_input_with_trust.cert_input.der_cert)));

    if (!x509_root) {
      PrintCertError("ERROR: X509Certificate::CreateFromBytes failed:",
                     cert_input_with_trust.cert_input);
    } else {
      scoped_test_roots.emplace_back(x509_root, cert_input_with_trust.trust);
    }
  }

  // TODO(mattm): add command line flags to configure VerifyFlags.
  int flags = 0;

  // TODO(crbug.com/40479281): use a real netlog and print the results?
  net::CertVerifyResult result;
  int rv = cert_verify_proc->Verify(
      x509_target_and_intermediates.get(), hostname,
      /*ocsp_response=*/std::string(), /*sct_list=*/std::string(), flags,
      &result, net::NetLogWithSource());

  std::cout << "CertVerifyProc result: " << net::ErrorToShortString(rv) << "\n";
  PrintCertVerifyResult(result);
  if (!dump_path.empty() && result.verified_cert) {
    if (!DumpX509CertificateChain(dump_path, result.verified_cert.get())) {
      return false;
    }
  }

  return rv == net::OK;
}
