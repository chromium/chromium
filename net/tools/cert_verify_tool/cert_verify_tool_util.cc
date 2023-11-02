// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/cert_verify_tool/cert_verify_tool_util.h"

#include <iostream>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "net/cert/pem.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"

#if BUILDFLAG(IS_MAC)
#include <Security/Security.h>

#include "base/strings/sys_string_conversions.h"
#include "net/cert/cert_verify_proc_mac.h"
#include "net/cert/internal/trust_store_mac.h"
#endif

namespace {

// The PEM block header used for PEM-encoded DER certificates.
const char kCertificateHeader[] = "CERTIFICATE";

// Parses |data_string| as a single DER cert or a PEM certificate list.
// This is an alternative to X509Certificate::CreateFrom[...] which
// is designed to decouple the file input and decoding from the DER Certificate
// parsing.
void ExtractCertificatesFromData(const std::string& data_string,
                                 const base::FilePath& file_path,
                                 std::vector<CertInput>* certs) {
  net::PEMTokenizer pem_tokenizer(data_string, {kCertificateHeader});
  int block = 0;
  while (pem_tokenizer.GetNext()) {
    CertInput cert;
    cert.der_cert = pem_tokenizer.data();
    cert.source_file_path = file_path;
    cert.source_details =
        base::StringPrintf("%s block %i", kCertificateHeader, block);
    certs->push_back(cert);
    ++block;
  }

  // If it was a PEM file, return the extracted results.
  if (block)
    return;

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> pkcs7_cert_buffers;
  if (net::x509_util::CreateCertBuffersFromPKCS7Bytes(
          base::as_bytes(base::make_span(data_string)), &pkcs7_cert_buffers)) {
    int n = 0;
    for (const auto& cert_buffer : pkcs7_cert_buffers) {
      CertInput cert;
      cert.der_cert = std::string(
          net::x509_util::CryptoBufferAsStringPiece(cert_buffer.get()));
      cert.source_file_path = file_path;
      cert.source_details = base::StringPrintf("PKCS #7 cert %i", n);
      certs->push_back(cert);
      ++n;
    }
    return;
  }

  // Otherwise, assume it is a single DER cert.
  CertInput cert;
  cert.der_cert = data_string;
  cert.source_file_path = file_path;
  certs->push_back(cert);
}

#if BUILDFLAG(IS_MAC)
std::string SecErrorStr(OSStatus err) {
  base::ScopedCFTypeRef<CFStringRef> cfstr(
      SecCopyErrorMessageString(err, nullptr));
  return base::StringPrintf("%d(%s)", err,
                            base::SysCFStringRefToUTF8(cfstr).c_str());
}

std::string TrustResultStr(uint32_t trust_result) {
  switch (trust_result) {
    case kSecTrustResultInvalid:
      return "kSecTrustResultInvalid";
    case kSecTrustResultProceed:
      return "kSecTrustResultProceed";
    case 2:  // kSecTrustResultConfirm SEC_DEPRECATED_ATTRIBUTE = 2,
      return "kSecTrustResultConfirm";
    case kSecTrustResultDeny:
      return "kSecTrustResultDeny";
    case kSecTrustResultUnspecified:
      return "kSecTrustResultUnspecified";
    case kSecTrustResultRecoverableTrustFailure:
      return "kSecTrustResultRecoverableTrustFailure";
    case kSecTrustResultFatalTrustFailure:
      return "kSecTrustResultFatalTrustFailure";
    case kSecTrustResultOtherError:
      return "kSecTrustResultOtherError";
    default:
      return "UNKNOWN";
  }
}
#endif

}  // namespace

bool ReadCertificatesFromFile(const base::FilePath& file_path,
                              std::vector<CertInput>* certs) {
  std::string file_data;
  if (!ReadFromFile(file_path, &file_data))
    return false;
  ExtractCertificatesFromData(file_data, file_path, certs);
  return true;
}

bool ReadChainFromFile(const base::FilePath& file_path,
                       CertInput* target,
                       std::vector<CertInput>* intermediates) {
  std::vector<CertInput> tmp_certs;
  if (!ReadCertificatesFromFile(file_path, &tmp_certs))
    return false;

  if (tmp_certs.empty())
    return true;

  *target = tmp_certs.front();

  intermediates->insert(intermediates->end(), ++tmp_certs.begin(),
                        tmp_certs.end());
  return true;
}

bool ReadFromFile(const base::FilePath& file_path, std::string* file_data) {
  if (!base::ReadFileToString(file_path, file_data)) {
    std::cerr << "ERROR: ReadFileToString " << file_path.value() << ": "
              << strerror(errno) << "\n";
    return false;
  }
  return true;
}

bool WriteToFile(const base::FilePath& file_path, const std::string& data) {
  if (base::WriteFile(file_path, data.data(), data.size()) < 0) {
    std::cerr << "ERROR: WriteFile " << file_path.value() << ": "
              << strerror(errno) << "\n";
    return false;
  }
  return true;
}

void PrintCertError(const std::string& error, const CertInput& cert) {
  std::cerr << error << " " << cert.source_file_path.value();
  if (!cert.source_details.empty())
    std::cerr << " (" << cert.source_details << ")";
  std::cerr << "\n";
}

void PrintDebugData(const base::SupportsUserData* debug_data) {
#if BUILDFLAG(IS_MAC)
  auto* mac_platform_debug_info =
      net::CertVerifyProcMac::ResultDebugData::Get(debug_data);
  if (mac_platform_debug_info) {
    std::cout << base::StringPrintf(
        "CertVerifyProcMac::ResultDebugData: trust_result=%u(%s) "
        "result_code=%s\n",
        mac_platform_debug_info->trust_result(),
        TrustResultStr(mac_platform_debug_info->trust_result()).c_str(),
        SecErrorStr(mac_platform_debug_info->result_code()).c_str());
    for (size_t i = 0; i < mac_platform_debug_info->status_chain().size();
         ++i) {
      const auto& cert_info = mac_platform_debug_info->status_chain()[i];
      std::string status_codes_str;
      for (const auto code : cert_info.status_codes) {
        if (!status_codes_str.empty())
          status_codes_str += ',';
        status_codes_str += SecErrorStr(code);
      }
      std::cout << base::StringPrintf(
          " cert %zu: status_bits=0x%x status_codes=%s\n", i,
          cert_info.status_bits, status_codes_str.c_str());
    }
  }

  auto* mac_trust_debug_info =
      net::TrustStoreMac::ResultDebugData::Get(debug_data);
  if (mac_trust_debug_info) {
    std::cout << base::StringPrintf(
        "TrustStoreMac::ResultDebugData::combined_trust_debug_info: 0x%x\n",
        mac_trust_debug_info->combined_trust_debug_info());
  }
#endif
}

std::string FingerPrintCryptoBuffer(const CRYPTO_BUFFER* cert_handle) {
  net::SHA256HashValue hash =
      net::X509Certificate::CalculateFingerprint256(cert_handle);
  return base::HexEncode(hash.data, std::size(hash.data));
}

std::string SubjectFromX509Certificate(const net::X509Certificate* cert) {
  return cert->subject().GetDisplayName();
}

std::string SubjectFromCryptoBuffer(CRYPTO_BUFFER* cert_handle) {
  scoped_refptr<net::X509Certificate> cert =
      net::X509Certificate::CreateFromBuffer(bssl::UpRef(cert_handle), {});
  if (!cert)
    return std::string();
  return SubjectFromX509Certificate(cert.get());
}
