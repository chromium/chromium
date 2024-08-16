// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_certificate.h"

#include <limits.h>
#include <stdlib.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "crypto/openssl_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/tracing.h"
#include "net/base/url_util.h"
#include "net/cert/asn1_util.h"
#include "net/cert/time_conversions.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/include/openssl/sha.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/name_constraints.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"
#include "third_party/boringssl/src/pki/parser.h"
#include "third_party/boringssl/src/pki/pem.h"
#include "third_party/boringssl/src/pki/signature_algorithm.h"
#include "third_party/boringssl/src/pki/verify_certificate_chain.h"
#include "third_party/boringssl/src/pki/verify_name_match.h"
#include "third_party/boringssl/src/pki/verify_signed_data.h"
#include "url/url_canon.h"

namespace net {

namespace {

// Indicates the order to use when trying to decode binary data, which is
// based on (speculation) as to what will be most common -> least common
constexpr auto kFormatDecodePriority = std::to_array<X509Certificate::Format>(
    {X509Certificate::FORMAT_SINGLE_CERTIFICATE,
     X509Certificate::FORMAT_PKCS7});

// The PEM block header used for DER certificates
const char kCertificateHeader[] = "CERTIFICATE";
// The PEM block header used for PKCS#7 data
const char kPKCS7Header[] = "PKCS7";

// Utility to split |src| on the first occurrence of |c|, if any. |right| will
// either be empty if |c| was not found, or will contain the remainder of the
// string including the split character itself.
void SplitOnChar(std::string_view src,
                 char c,
                 std::string_view* left,
                 std::string_view* right) {
  size_t pos = src.find(c);
  if (pos == std::string_view::npos) {
    *left = src;
    *right = std::string_view();
  } else {
    *left = src.substr(0, pos);
    *right = src.substr(pos);
  }
}

// Sets |value| to the Value from a DER Sequence Tag-Length-Value and return
// true, or return false if the TLV was not a valid DER Sequence.
[[nodiscard]] bool ParseSequenceValue(const bssl::der::Input& tlv,
                                      bssl::der::Input* value) {
  bssl::der::Parser parser(tlv);
  return parser.ReadTag(CBS_ASN1_SEQUENCE, value) && !parser.HasMore();
}

// Normalize |cert|'s Issuer and store it in |out_normalized_issuer|, returning
// true on success or false if there was a parsing error.
bool GetNormalizedCertIssuer(CRYPTO_BUFFER* cert,
                             std::string* out_normalized_issuer) {
  bssl::der::Input tbs_certificate_tlv;
  bssl::der::Input signature_algorithm_tlv;
  bssl::der::BitString signature_value;
  if (!bssl::ParseCertificate(
          bssl::der::Input(x509_util::CryptoBufferAsSpan(cert)),
          &tbs_certificate_tlv, &signature_algorithm_tlv, &signature_value,
          nullptr)) {
    return false;
  }
  bssl::ParsedTbsCertificate tbs;
  if (!ParseTbsCertificate(tbs_certificate_tlv,
                           x509_util::DefaultParseCertificateOptions(), &tbs,
                           nullptr))
    return false;

  bssl::der::Input issuer_value;
  if (!ParseSequenceValue(tbs.issuer_tlv, &issuer_value))
    return false;

  bssl::CertErrors errors;
  return NormalizeName(issuer_value, out_normalized_issuer, &errors);
}

bssl::UniquePtr<CRYPTO_BUFFER> CreateCertBufferFromBytesWithSanityCheck(
    base::span<const uint8_t> data) {
  bssl::der::Input tbs_certificate_tlv;
  bssl::der::Input signature_algorithm_tlv;
  bssl::der::BitString signature_value;
  // Do a bare minimum of DER parsing here to see if the input looks
  // certificate-ish.
  if (!bssl::ParseCertificate(bssl::der::Input(data), &tbs_certificate_tlv,
                              &signature_algorithm_tlv, &signature_value,
                              nullptr)) {
    return nullptr;
  }
  return x509_util::CreateCryptoBuffer(data);
}

}  // namespace

// static
scoped_refptr<X509Certificate> X509Certificate::CreateFromBuffer(
    bssl::UniquePtr<CRYPTO_BUFFER> cert_buffer,
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates) {
  return CreateFromBufferUnsafeOptions(std::move(cert_buffer),
                                       std::move(intermediates), {});
}

// static
scoped_refptr<X509Certificate> X509Certificate::CreateFromBufferUnsafeOptions(
    bssl::UniquePtr<CRYPTO_BUFFER> cert_buffer,
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates,
    UnsafeCreateOptions options) {
  DCHECK(cert_buffer);
  ParsedFields parsed;
  if (!parsed.Initialize(cert_buffer.get(), options)) {
    return nullptr;
  }
  return base::WrapRefCounted(new X509Certificate(
      std::move(parsed), std::move(cert_buffer), std::move(intermediates)));
}

// static
scoped_refptr<X509Certificate> X509Certificate::CreateFromDERCertChain(
    const std::vector<std::string_view>& der_certs) {
  return CreateFromDERCertChainUnsafeOptions(der_certs, {});
}

// static
scoped_refptr<X509Certificate>
X509Certificate::CreateFromDERCertChainUnsafeOptions(
    const std::vector<std::string_view>& der_certs,
    UnsafeCreateOptions options) {
  TRACE_EVENT0("io", "X509Certificate::CreateFromDERCertChain");
  if (der_certs.empty())
    return nullptr;

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediate_ca_certs;
  intermediate_ca_certs.reserve(der_certs.size() - 1);
  for (size_t i = 1; i < der_certs.size(); i++) {
    intermediate_ca_certs.push_back(
        x509_util::CreateCryptoBuffer(der_certs[i]));
  }

  return CreateFromBufferUnsafeOptions(
      x509_util::CreateCryptoBuffer(der_certs[0]),
      std::move(intermediate_ca_certs), options);
}

// static
scoped_refptr<X509Certificate> X509Certificate::CreateFromBytes(
    base::span<const uint8_t> data) {
  return CreateFromBytesUnsafeOptions(data, {});
}

// static
scoped_refptr<X509Certificate> X509Certificate::CreateFromBytesUnsafeOptions(
    base::span<const uint8_t> data,
    UnsafeCreateOptions options) {
  scoped_refptr<X509Certificate> cert = CreateFromBufferUnsafeOptions(
      x509_util::CreateCryptoBuffer(data), {}, options);
  return cert;
}

// static
scoped_refptr<X509Certificate> X509Certificate::CreateFromPickle(
    base::PickleIterator* pickle_iter) {
  return CreateFromPickleUnsafeOptions(pickle_iter, {});
}

// static
scoped_refptr<X509Certificate> X509Certificate::CreateFromPickleUnsafeOptions(
    base::PickleIterator* pickle_iter,
    UnsafeCreateOptions options) {
  size_t chain_length = 0;
  if (!pickle_iter->ReadLength(&chain_length))
    return nullptr;

  std::vector<std::string_view> cert_chain;
  const char* data = nullptr;
  size_t data_length = 0;
  for (size_t i = 0; i < chain_length; ++i) {
    if (!pickle_iter->ReadData(&data, &data_length))
      return nullptr;
    cert_chain.emplace_back(data, data_length);
  }
  return CreateFromDERCertChainUnsafeOptions(cert_chain, options);
}

// static
CertificateList X509Certificate::CreateCertificateListFromBytes(
    base::span<const uint8_t> data,
    int format) {
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certificates;

  // Check to see if it is in a PEM-encoded form. This check is performed
  // first, as both OS X and NSS will both try to convert if they detect
  // PEM encoding, except they don't do it consistently between the two.
  std::vector<std::string> pem_headers;

  // To maintain compatibility with NSS/Firefox, CERTIFICATE is a universally
  // valid PEM block header for any format.
  pem_headers.push_back(kCertificateHeader);
  if (format & FORMAT_PKCS7)
    pem_headers.push_back(kPKCS7Header);

  bssl::PEMTokenizer pem_tokenizer(base::as_string_view(data), pem_headers);
  while (pem_tokenizer.GetNext()) {
    std::string decoded(pem_tokenizer.data());

    bssl::UniquePtr<CRYPTO_BUFFER> handle;
    if (format & FORMAT_PEM_CERT_SEQUENCE) {
      handle =
          CreateCertBufferFromBytesWithSanityCheck(base::as_byte_span(decoded));
    }
    if (handle) {
      // Parsed a DER encoded certificate. All PEM blocks that follow must
      // also be DER encoded certificates wrapped inside of PEM blocks.
      format = FORMAT_PEM_CERT_SEQUENCE;
      certificates.push_back(std::move(handle));
      continue;
    }

    // If the first block failed to parse as a DER certificate, and
    // formats other than PEM are acceptable, check to see if the decoded
    // data is one of the accepted formats.
    if (format & ~FORMAT_PEM_CERT_SEQUENCE) {
      for (size_t i = 0;
           certificates.empty() && i < std::size(kFormatDecodePriority); ++i) {
        if (format & kFormatDecodePriority[i]) {
          certificates = CreateCertBuffersFromBytes(base::as_byte_span(decoded),
                                                    kFormatDecodePriority[i]);
        }
      }
    }

    // Stop parsing after the first block for any format but a sequence of
    // PEM-encoded DER certificates. The case of FORMAT_PEM_CERT_SEQUENCE
    // is handled above, and continues processing until a certificate fails
    // to parse.
    break;
  }

  // Try each of the formats, in order of parse preference, to see if |data|
  // contains the binary representation of a Format, if it failed to parse
  // as a PEM certificate/chain.
  for (size_t i = 0;
       certificates.empty() && i < std::size(kFormatDecodePriority); ++i) {
    if (format & kFormatDecodePriority[i])
      certificates = CreateCertBuffersFromBytes(data, kFormatDecodePriority[i]);
  }

  CertificateList results;
  // No certificates parsed.
  if (certificates.empty())
    return results;

  for (auto& it : certificates) {
    scoped_refptr<X509Certificate> cert = CreateFromBuffer(std::move(it), {});
    if (cert)
      results.push_back(std::move(cert));
  }

  return results;
}

scoped_refptr<X509Certificate> X509Certificate::CloneWithDifferentIntermediates(
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates) {
  // If intermediates are the same, return another reference to the same
  // object. Note that this only does a pointer equality comparison on the
  // CRYPTO_BUFFERs, which is generally sufficient, but in some edge cases
  // buffers have equal contents but with different addresses. This is
  // acceptable as this is just an optimization.
  if (intermediates == intermediate_ca_certs_) {
    return this;
  }

  return base::WrapRefCounted(
      new X509Certificate(*this, std::move(intermediates)));
}

void X509Certificate::Persist(base::Pickle* pickle) const {
  DCHECK(cert_buffer_);
  // This would be an absolutely insane number of intermediates.
  if (intermediate_ca_certs_.size() > static_cast<size_t>(INT_MAX) - 1) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  pickle->WriteInt(static_cast<int>(intermediate_ca_certs_.size() + 1));
  pickle->WriteString(x509_util::CryptoBufferAsStringPiece(cert_buffer_.get()));
  for (const auto& intermediate : intermediate_ca_certs_) {
    pickle->WriteString(
        x509_util::CryptoBufferAsStringPiece(intermediate.get()));
  }
}

bool X509Certificate::GetSubjectAltName(
    std::vector<std::string>* dns_names,
    std::vector<std::string>* ip_addrs) const {
  if (dns_names)
    dns_names->clear();
  if (ip_addrs)
    ip_addrs->clear();

  bssl::der::Input tbs_certificate_tlv;
  bssl::der::Input signature_algorithm_tlv;
  bssl::der::BitString signature_value;
  if (!bssl::ParseCertificate(bssl::der::Input(cert_span()),
                              &tbs_certificate_tlv, &signature_algorithm_tlv,
                              &signature_value, nullptr)) {
    return false;
  }

  bssl::ParsedTbsCertificate tbs;
  if (!ParseTbsCertificate(tbs_certificate_tlv,
                           x509_util::DefaultParseCertificateOptions(), &tbs,
                           nullptr))
    return false;
  if (!tbs.extensions_tlv)
    return false;

  std::map<bssl::der::Input, bssl::ParsedExtension> extensions;
  if (!ParseExtensions(tbs.extensions_tlv.value(), &extensions))
    return false;

  bssl::ParsedExtension subject_alt_names_extension;
  if (!ConsumeExtension(bssl::der::Input(bssl::kSubjectAltNameOid), &extensions,
                        &subject_alt_names_extension)) {
    return false;
  }

  bssl::CertErrors errors;
  std::unique_ptr<bssl::GeneralNames> subject_alt_names =
      bssl::GeneralNames::Create(subject_alt_names_extension.value, &errors);
  if (!subject_alt_names)
    return false;

  if (dns_names) {
    for (const auto& dns_name : subject_alt_names->dns_names)
      dns_names->push_back(std::string(dns_name));
  }
  if (ip_addrs) {
    for (const auto& addr : subject_alt_names->ip_addresses) {
      ip_addrs->push_back(std::string(addr.AsStringView()));
    }
  }

  return !subject_alt_names->dns_names.empty() ||
         !subject_alt_names->ip_addresses.empty();
}

bool X509Certificate::HasExpired() const {
  return base::Time::Now() > valid_expiry();
}

bool X509Certificate::EqualsExcludingChain(const X509Certificate* other) const {
  return x509_util::CryptoBufferEqual(cert_buffer_.get(),
                                      other->cert_buffer_.get());
}

bool X509Certificate::EqualsIncludingChain(const X509Certificate* other) const {
  if (intermediate_ca_certs_.size() != other->intermediate_ca_certs_.size() ||
      !EqualsExcludingChain(other)) {
    return false;
  }
  for (size_t i = 0; i < intermediate_ca_certs_.size(); ++i) {
    if (!x509_util::CryptoBufferEqual(intermediate_ca_certs_[i].get(),
                                      other->intermediate_ca_certs_[i].get())) {
      return false;
    }
  }
  return true;
}

bool X509Certificate::IsIssuedByEncoded(
    const std::vector<std::string>& valid_issuers) const {
  std::vector<std::string> normalized_issuers;
  bssl::CertErrors errors;
  for (const auto& raw_issuer : valid_issuers) {
    bssl::der::Input issuer_value;
    std::string normalized_issuer;
    if (!ParseSequenceValue(bssl::der::Input(raw_issuer), &issuer_value) ||
        !NormalizeName(issuer_value, &normalized_issuer, &errors)) {
      continue;
    }
    normalized_issuers.push_back(std::move(normalized_issuer));
  }

  std::string normalized_cert_issuer;
  if (!GetNormalizedCertIssuer(cert_buffer_.get(), &normalized_cert_issuer))
    return false;
  if (base::Contains(normalized_issuers, normalized_cert_issuer))
    return true;

  for (const auto& intermediate : intermediate_ca_certs_) {
    if (!GetNormalizedCertIssuer(intermediate.get(), &normalized_cert_issuer))
      return false;
    if (base::Contains(normalized_issuers, normalized_cert_issuer))
      return true;
  }
  return false;
}

// static
bool X509Certificate::VerifyHostname(
    std::string_view hostname,
    const std::vector<std::string>& cert_san_dns_names,
    const std::vector<std::string>& cert_san_ip_addrs) {
  DCHECK(!hostname.empty());

  if (cert_san_dns_names.empty() && cert_san_ip_addrs.empty()) {
    // Either a dNSName or iPAddress subjectAltName MUST be present in order
    // to match, so fail quickly if not.
    return false;
  }

  // Perform name verification following http://tools.ietf.org/html/rfc6125.
  // The terminology used in this method is as per that RFC:-
  // Reference identifier == the host the local user/agent is intending to
  //                         access, i.e. the thing displayed in the URL bar.
  // Presented identifier(s) == name(s) the server knows itself as, in its cert.

  // CanonicalizeHost requires surrounding brackets to parse an IPv6 address.
  const std::string host_or_ip = hostname.find(':') != std::string::npos
                                     ? base::StrCat({"[", hostname, "]"})
                                     : std::string(hostname);
  url::CanonHostInfo host_info;
  std::string reference_name = CanonicalizeHost(host_or_ip, &host_info);

  // If the host cannot be canonicalized, fail fast.
  if (reference_name.empty())
    return false;

  // Fully handle all cases where |hostname| contains an IP address.
  if (host_info.IsIPAddress()) {
    std::string_view ip_addr_string(
        reinterpret_cast<const char*>(host_info.address),
        host_info.AddressLength());
    return base::Contains(cert_san_ip_addrs, ip_addr_string);
  }

  // The host portion of a URL may support a variety of name resolution formats
  // and services. However, the only supported name types in this code are IP
  // addresses, which have been handled above via iPAddress subjectAltNames,
  // and DNS names, via dNSName subjectAltNames.
  // Validate that the host conforms to the DNS preferred name syntax, in
  // either relative or absolute form, and exclude the "root" label for DNS.
  if (reference_name == "." || !IsCanonicalizedHostCompliant(reference_name))
    return false;

  // CanonicalizeHost does not normalize absolute vs relative DNS names. If
  // the input name was absolute (included trailing .), normalize it as if it
  // was relative.
  if (reference_name.back() == '.')
    reference_name.pop_back();

  // |reference_domain| is the remainder of |host| after the leading host
  // component is stripped off, but includes the leading dot e.g.
  // "www.f.com" -> ".f.com".
  // If there is no meaningful domain part to |host| (e.g. it contains no dots)
  // then |reference_domain| will be empty.
  std::string_view reference_host, reference_domain;
  SplitOnChar(reference_name, '.', &reference_host, &reference_domain);
  bool allow_wildcards = false;
  if (!reference_domain.empty()) {
    DCHECK(reference_domain.starts_with("."));

    // Do not allow wildcards for public/ICANN registry controlled domains -
    // that is, prevent *.com or *.co.uk as valid presented names, but do not
    // prevent *.appspot.com (a private registry controlled domain).
    // In addition, unknown top-level domains (such as 'intranet' domains or
    // new TLDs/gTLDs not yet added to the registry controlled domain dataset)
    // are also implicitly prevented.
    // Because |reference_domain| must contain at least one name component that
    // is not registry controlled, this ensures that all reference domains
    // contain at least three domain components when using wildcards.
    size_t registry_length =
        registry_controlled_domains::GetCanonicalHostRegistryLength(
            reference_name,
            registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
            registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);

    // Because |reference_name| was already canonicalized, the following
    // should never happen.
    CHECK_NE(std::string::npos, registry_length);

    // Account for the leading dot in |reference_domain|.
    bool is_registry_controlled =
        registry_length != 0 &&
        registry_length == (reference_domain.size() - 1);

    // Additionally, do not attempt wildcard matching for purely numeric
    // hostnames.
    allow_wildcards =
        !is_registry_controlled &&
        reference_name.find_first_not_of("0123456789.") != std::string::npos;
  }

  // Now step through the DNS names doing wild card comparison (if necessary)
  // on each against the reference name.
  for (const auto& cert_san_dns_name : cert_san_dns_names) {
    // Catch badly corrupt cert names up front.
    if (cert_san_dns_name.empty() ||
        cert_san_dns_name.find('\0') != std::string::npos) {
      continue;
    }
    std::string presented_name(base::ToLowerASCII(cert_san_dns_name));

    // Remove trailing dot, if any.
    if (*presented_name.rbegin() == '.')
      presented_name.resize(presented_name.length() - 1);

    // The hostname must be at least as long as the cert name it is matching,
    // as we require the wildcard (if present) to match at least one character.
    if (presented_name.length() > reference_name.length())
      continue;

    std::string_view presented_host, presented_domain;
    SplitOnChar(presented_name, '.', &presented_host, &presented_domain);

    if (presented_domain != reference_domain)
      continue;

    if (presented_host != "*") {
      if (presented_host == reference_host)
        return true;
      continue;
    }

    if (!allow_wildcards)
      continue;

    return true;
  }
  return false;
}

bool X509Certificate::VerifyNameMatch(std::string_view hostname) const {
  std::vector<std::string> dns_names, ip_addrs;
  GetSubjectAltName(&dns_names, &ip_addrs);
  return VerifyHostname(hostname, dns_names, ip_addrs);
}

// static
bool X509Certificate::GetPEMEncodedFromDER(std::string_view der_encoded,
                                           std::string* pem_encoded) {
  if (der_encoded.empty())
    return false;

  *pem_encoded = bssl::PEMEncode(der_encoded, "CERTIFICATE");
  return true;
}

// static
bool X509Certificate::GetPEMEncoded(const CRYPTO_BUFFER* cert_buffer,
                                    std::string* pem_encoded) {
  return GetPEMEncodedFromDER(x509_util::CryptoBufferAsStringPiece(cert_buffer),
                              pem_encoded);
}

bool X509Certificate::GetPEMEncodedChain(
    std::vector<std::string>* pem_encoded) const {
  std::vector<std::string> encoded_chain;
  std::string pem_data;
  if (!GetPEMEncoded(cert_buffer(), &pem_data))
    return false;
  encoded_chain.push_back(pem_data);
  for (const auto& intermediate_ca_cert : intermediate_ca_certs_) {
    if (!GetPEMEncoded(intermediate_ca_cert.get(), &pem_data))
      return false;
    encoded_chain.push_back(pem_data);
  }
  pem_encoded->swap(encoded_chain);
  return true;
}

// static
void X509Certificate::GetPublicKeyInfo(const CRYPTO_BUFFER* cert_buffer,
                                       size_t* size_bits,
                                       PublicKeyType* type) {
  *type = kPublicKeyTypeUnknown;
  *size_bits = 0;

  std::string_view spki;
  if (!asn1::ExtractSPKIFromDERCert(
          x509_util::CryptoBufferAsStringPiece(cert_buffer), &spki)) {
    return;
  }

  bssl::UniquePtr<EVP_PKEY> pkey;
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(spki.data()), spki.size());
  pkey.reset(EVP_parse_public_key(&cbs));
  if (!pkey)
    return;

  switch (EVP_PKEY_id(pkey.get())) {
    case EVP_PKEY_RSA:
      *type = kPublicKeyTypeRSA;
      break;
    case EVP_PKEY_EC:
      *type = kPublicKeyTypeECDSA;
      break;
  }
  *size_bits = base::saturated_cast<size_t>(EVP_PKEY_bits(pkey.get()));
}

// static
std::vector<bssl::UniquePtr<CRYPTO_BUFFER>>
X509Certificate::CreateCertBuffersFromBytes(base::span<const uint8_t> data,
                                            Format format) {
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> results;

  switch (format) {
    case FORMAT_SINGLE_CERTIFICATE: {
      bssl::UniquePtr<CRYPTO_BUFFER> handle =
          CreateCertBufferFromBytesWithSanityCheck(data);
      if (handle)
        results.push_back(std::move(handle));
      break;
    }
    case FORMAT_PKCS7: {
      x509_util::CreateCertBuffersFromPKCS7Bytes(data, &results);
      break;
    }
    default: {
      NOTREACHED_IN_MIGRATION()
          << "Certificate format " << format << " unimplemented";
      break;
    }
  }

  return results;
}

// static
SHA256HashValue X509Certificate::CalculateFingerprint256(
    const CRYPTO_BUFFER* cert) {
  SHA256HashValue sha256;

  SHA256(CRYPTO_BUFFER_data(cert), CRYPTO_BUFFER_len(cert), sha256.data);
  return sha256;
}

SHA256HashValue X509Certificate::CalculateChainFingerprint256() const {
  SHA256HashValue sha256;
  memset(sha256.data, 0, sizeof(sha256.data));

  SHA256_CTX sha256_ctx;
  SHA256_Init(&sha256_ctx);
  SHA256_Update(&sha256_ctx, CRYPTO_BUFFER_data(cert_buffer_.get()),
                CRYPTO_BUFFER_len(cert_buffer_.get()));
  for (const auto& cert : intermediate_ca_certs_) {
    SHA256_Update(&sha256_ctx, CRYPTO_BUFFER_data(cert.get()),
                  CRYPTO_BUFFER_len(cert.get()));
  }
  SHA256_Final(sha256.data, &sha256_ctx);

  return sha256;
}

// static
bool X509Certificate::IsSelfSigned(CRYPTO_BUFFER* cert_buffer) {
  std::shared_ptr<const bssl::ParsedCertificate> parsed_cert =
      bssl::ParsedCertificate::Create(
          bssl::UpRef(cert_buffer), x509_util::DefaultParseCertificateOptions(),
          /*errors=*/nullptr);
  if (!parsed_cert) {
    return false;
  }
  return VerifyCertificateIsSelfSigned(*parsed_cert, /*cache=*/nullptr,
                                       /*errors=*/nullptr);
}

X509Certificate::X509Certificate(
    ParsedFields parsed,
    bssl::UniquePtr<CRYPTO_BUFFER> cert_buffer,
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates)
    : parsed_(std::move(parsed)),
      cert_buffer_(std::move(cert_buffer)),
      intermediate_ca_certs_(std::move(intermediates)) {}

X509Certificate::X509Certificate(
    const X509Certificate& other,
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates)
    : parsed_(other.parsed_),
      cert_buffer_(bssl::UpRef(other.cert_buffer_)),
      intermediate_ca_certs_(std::move(intermediates)) {}

X509Certificate::~X509Certificate() = default;

base::span<const uint8_t> X509Certificate::cert_span() const {
  return x509_util::CryptoBufferAsSpan(cert_buffer_.get());
}

X509Certificate::ParsedFields::ParsedFields() = default;
X509Certificate::ParsedFields::ParsedFields(const ParsedFields&) = default;
X509Certificate::ParsedFields::ParsedFields(ParsedFields&&) = default;
X509Certificate::ParsedFields::~ParsedFields() = default;

bool X509Certificate::ParsedFields::Initialize(
    const CRYPTO_BUFFER* cert_buffer,
    X509Certificate::UnsafeCreateOptions options) {
  bssl::der::Input tbs_certificate_tlv;
  bssl::der::Input signature_algorithm_tlv;
  bssl::der::BitString signature_value;

  if (!bssl::ParseCertificate(
          bssl::der::Input(x509_util::CryptoBufferAsSpan(cert_buffer)),
          &tbs_certificate_tlv, &signature_algorithm_tlv, &signature_value,
          nullptr)) {
    return false;
  }

  bssl::ParsedTbsCertificate tbs;
  if (!ParseTbsCertificate(tbs_certificate_tlv,
                           x509_util::DefaultParseCertificateOptions(), &tbs,
                           nullptr))
    return false;

  CertPrincipal::PrintableStringHandling printable_string_handling =
      options.printable_string_is_utf8
          ? CertPrincipal::PrintableStringHandling::kAsUTF8Hack
          : CertPrincipal::PrintableStringHandling::kDefault;
  if (!subject_.ParseDistinguishedName(tbs.subject_tlv,
                                       printable_string_handling) ||
      !issuer_.ParseDistinguishedName(tbs.issuer_tlv,
                                      printable_string_handling)) {
    return false;
  }

  if (!GeneralizedTimeToTime(tbs.validity_not_before, &valid_start_) ||
      !GeneralizedTimeToTime(tbs.validity_not_after, &valid_expiry_)) {
    return false;
  }
  serial_number_ = tbs.serial_number.AsString();
  return true;
}

}  // namespace net
