// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/client_cert_store_mac.h"

#include <CommonCrypto/CommonDigest.h>
#include <CoreFoundation/CFArray.h>
#include <CoreServices/CoreServices.h>
#include <Security/SecBase.h>
#include <Security/Security.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/lock.h"
#include "crypto/mac_security_services_lock.h"
#include "net/base/host_port_pair.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_apple.h"
#include "net/ssl/client_cert_identity_mac.h"
#include "net/ssl/ssl_platform_key_util.h"
#include "third_party/boringssl/src/pki/extended_key_usage.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"

using base::apple::ScopedCFTypeRef;

namespace net {

namespace {

using ClientCertIdentityMacList =
    std::vector<std::unique_ptr<ClientCertIdentityMac>>;

// Gets the issuer for a given cert, starting with the cert itself and
// including the intermediate and finally root certificates (if any).
// This function calls SecTrust but doesn't actually pay attention to the trust
// result: it shouldn't be used to determine trust, just to traverse the chain.
OSStatus CopyCertChain(
    SecCertificateRef cert_handle,
    base::apple::ScopedCFTypeRef<CFArrayRef>* out_cert_chain) {
  DCHECK(cert_handle);
  DCHECK(out_cert_chain);

  // Create an SSL policy ref configured for client cert evaluation.
  ScopedCFTypeRef<SecPolicyRef> ssl_policy(
      SecPolicyCreateSSL(/*server=*/false, /*hostname=*/nullptr));
  if (!ssl_policy)
    return errSecNoPolicyModule;

  // Create a SecTrustRef.
  ScopedCFTypeRef<CFArrayRef> input_certs(CFArrayCreate(
      nullptr, const_cast<const void**>(reinterpret_cast<void**>(&cert_handle)),
      1, &kCFTypeArrayCallBacks));
  OSStatus result;
  SecTrustRef trust_ref = nullptr;
  {
    base::AutoLock lock(crypto::GetMacSecurityServicesLock());
    result = SecTrustCreateWithCertificates(input_certs.get(), ssl_policy.get(),
                                            &trust_ref);
  }
  if (result)
    return result;
  ScopedCFTypeRef<SecTrustRef> trust(trust_ref);

  // Evaluate trust, which creates the cert chain.
  {
    base::AutoLock lock(crypto::GetMacSecurityServicesLock());
    // The return value is intentionally ignored since we only care about
    // building a cert chain, not whether it is trusted (the server is the
    // only one that can decide that.)
    std::ignore = SecTrustEvaluateWithError(trust.get(), nullptr);
    *out_cert_chain = x509_util::CertificateChainFromSecTrust(trust.get());
  }
  return result;
}

// Returns true if |*identity| is issued by an authority in |valid_issuers|
// according to Keychain Services, rather than using |identity|'s intermediate
// certificates. If it is, |*identity| is updated to include the intermediates.
bool IsIssuedByInKeychain(const std::vector<std::string>& valid_issuers,
                          ClientCertIdentityMac* identity) {
  DCHECK(identity);
  DCHECK(identity->sec_identity_ref());

  ScopedCFTypeRef<SecCertificateRef> os_cert;
  int err = SecIdentityCopyCertificate(identity->sec_identity_ref(),
                                       os_cert.InitializeInto());
  if (err != noErr)
    return false;
  base::apple::ScopedCFTypeRef<CFArrayRef> cert_chain;
  OSStatus result = CopyCertChain(os_cert.get(), &cert_chain);
  if (result) {
    OSSTATUS_LOG(ERROR, result) << "CopyCertChain error";
    return false;
  }

  if (!cert_chain)
    return false;

  std::vector<base::apple::ScopedCFTypeRef<SecCertificateRef>> intermediates;
  for (CFIndex i = 1, chain_count = CFArrayGetCount(cert_chain.get());
       i < chain_count; ++i) {
    SecCertificateRef sec_cert = reinterpret_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(cert_chain.get(), i)));
    intermediates.emplace_back(sec_cert, base::scoped_policy::RETAIN);
  }

  // Allow UTF-8 inside PrintableStrings in client certificates. See
  // crbug.com/770323.
  X509Certificate::UnsafeCreateOptions options;
  options.printable_string_is_utf8 = true;
  scoped_refptr<X509Certificate> new_cert(
      x509_util::CreateX509CertificateFromSecCertificate(os_cert, intermediates,
                                                         options));

  if (!new_cert || !new_cert->IsIssuedByEncoded(valid_issuers))
    return false;

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediate_buffers;
  intermediate_buffers.reserve(new_cert->intermediate_buffers().size());
  for (const auto& intermediate : new_cert->intermediate_buffers()) {
    intermediate_buffers.push_back(bssl::UpRef(intermediate.get()));
  }
  identity->SetIntermediates(std::move(intermediate_buffers));
  return true;
}

// Does |cert|'s usage allow SSL client authentication?
bool SupportsSSLClientAuth(CRYPTO_BUFFER* cert) {
  DCHECK(cert);

  bssl::ParseCertificateOptions options;
  options.allow_invalid_serial_numbers = true;
  bssl::der::Input tbs_certificate_tlv;
  bssl::der::Input signature_algorithm_tlv;
  bssl::der::BitString signature_value;
  bssl::ParsedTbsCertificate tbs;
  if (!bssl::ParseCertificate(
          bssl::der::Input(CRYPTO_BUFFER_data(cert), CRYPTO_BUFFER_len(cert)),
          &tbs_certificate_tlv, &signature_algorithm_tlv, &signature_value,
          nullptr /* errors*/) ||
      !ParseTbsCertificate(tbs_certificate_tlv, options, &tbs,
                           nullptr /*errors*/)) {
    return false;
  }

  if (!tbs.extensions_tlv)
    return true;

  std::map<bssl::der::Input, bssl::ParsedExtension> extensions;
  if (!ParseExtensions(tbs.extensions_tlv.value(), &extensions))
    return false;

  // RFC5280 says to take the intersection of the two extensions.
  //
  // We only support signature-based client certificates, so we need the
  // digitalSignature bit.
  //
  // In particular, if a key has the nonRepudiation bit and not the
  // digitalSignature one, we will not offer it to the user.
  if (auto it = extensions.find(bssl::der::Input(bssl::kKeyUsageOid));
      it != extensions.end()) {
    bssl::der::BitString key_usage;
    if (!bssl::ParseKeyUsage(it->second.value, &key_usage) ||
        !key_usage.AssertsBit(bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE)) {
      return false;
    }
  }

  if (auto it = extensions.find(bssl::der::Input(bssl::kExtKeyUsageOid));
      it != extensions.end()) {
    std::vector<bssl::der::Input> extended_key_usage;
    if (!bssl::ParseEKUExtension(it->second.value, &extended_key_usage)) {
      return false;
    }
    bool found_acceptable_eku = false;
    for (const auto& oid : extended_key_usage) {
      if (oid == bssl::der::Input(bssl::kAnyEKU) ||
          oid == bssl::der::Input(bssl::kClientAuth)) {
        found_acceptable_eku = true;
        break;
      }
    }
    if (!found_acceptable_eku)
      return false;
  }

  return true;
}

// Examines the certificates in |preferred_identity| and |regular_identities| to
// find all certificates that match the client certificate request in |request|,
// storing the matching certificates in |selected_identities|.
// If |query_keychain| is true, Keychain Services will be queried to construct
// full certificate chains. If it is false, only the the certificates and their
// intermediates (available via X509Certificate::intermediate_buffers())
// will be considered.
void GetClientCertsImpl(
    std::unique_ptr<ClientCertIdentityMac> preferred_identity,
    ClientCertIdentityMacList regular_identities,
    const SSLCertRequestInfo& request,
    bool query_keychain,
    ClientCertIdentityList* selected_identities) {
  scoped_refptr<X509Certificate> preferred_cert_orig;
  ClientCertIdentityMacList preliminary_list = std::move(regular_identities);
  if (preferred_identity) {
    preferred_cert_orig = preferred_identity->certificate();
    preliminary_list.insert(preliminary_list.begin(),
                            std::move(preferred_identity));
  }

  selected_identities->clear();
  for (size_t i = 0; i < preliminary_list.size(); ++i) {
    std::unique_ptr<ClientCertIdentityMac>& cert = preliminary_list[i];
    if (cert->certificate()->HasExpired() ||
        !SupportsSSLClientAuth(cert->certificate()->cert_buffer())) {
      continue;
    }

    // Skip duplicates (a cert may be in multiple keychains).
    if (base::ranges::any_of(
            *selected_identities,
            [&cert](const std::unique_ptr<ClientCertIdentity>&
                        other_cert_identity) {
              return x509_util::CryptoBufferEqual(
                  cert->certificate()->cert_buffer(),
                  other_cert_identity->certificate()->cert_buffer());
            })) {
      continue;
    }

    // Check if the certificate issuer is allowed by the server.
    if (request.cert_authorities.empty() ||
        cert->certificate()->IsIssuedByEncoded(request.cert_authorities) ||
        (query_keychain &&
         IsIssuedByInKeychain(request.cert_authorities, cert.get()))) {
      selected_identities->push_back(std::move(cert));
    }
  }

  // Preferred cert should appear first in the ui, so exclude it from the
  // sorting.  Compare the cert_buffer since the X509Certificate object may
  // have changed if intermediates were added.
  ClientCertIdentityList::iterator sort_begin = selected_identities->begin();
  ClientCertIdentityList::iterator sort_end = selected_identities->end();
  if (preferred_cert_orig && sort_begin != sort_end &&
      x509_util::CryptoBufferEqual(
          sort_begin->get()->certificate()->cert_buffer(),
          preferred_cert_orig->cert_buffer())) {
    ++sort_begin;
  }
  sort(sort_begin, sort_end, ClientCertIdentitySorter());
}

// Given a |sec_identity|, identifies its corresponding certificate, and either
// adds it to |regular_identities| or assigns it to |preferred_identity|, if the
// |sec_identity| matches the |preferred_sec_identity|.
void AddIdentity(ScopedCFTypeRef<SecIdentityRef> sec_identity,
                 SecIdentityRef preferred_sec_identity,
                 ClientCertIdentityMacList* regular_identities,
                 std::unique_ptr<ClientCertIdentityMac>* preferred_identity) {
  OSStatus err;
  ScopedCFTypeRef<SecCertificateRef> cert_handle;
  err = SecIdentityCopyCertificate(sec_identity.get(),
                                   cert_handle.InitializeInto());
  if (err != noErr)
    return;

  // Allow UTF-8 inside PrintableStrings in client certificates. See
  // crbug.com/770323.
  X509Certificate::UnsafeCreateOptions options;
  options.printable_string_is_utf8 = true;
  scoped_refptr<X509Certificate> cert(
      x509_util::CreateX509CertificateFromSecCertificate(cert_handle, {},
                                                         options));
  if (!cert)
    return;

  if (preferred_sec_identity &&
      CFEqual(preferred_sec_identity, sec_identity.get())) {
    *preferred_identity = std::make_unique<ClientCertIdentityMac>(
        std::move(cert), std::move(sec_identity));
  } else {
    regular_identities->push_back(std::make_unique<ClientCertIdentityMac>(
        std::move(cert), std::move(sec_identity)));
  }
}

ClientCertIdentityList GetClientCertsOnBackgroundThread(
    scoped_refptr<const SSLCertRequestInfo> request) {
  std::string server_domain = request->host_and_port.host();

  ScopedCFTypeRef<SecIdentityRef> preferred_sec_identity;
  if (!server_domain.empty()) {
    // See if there's an identity preference for this domain:
    ScopedCFTypeRef<CFStringRef> domain_str(
        base::SysUTF8ToCFStringRef("https://" + server_domain));
    // While SecIdentityCopyPreferred appears to take a list of CA issuers
    // to restrict the identity search to, within Security.framework the
    // argument is ignored and filtering unimplemented. See SecIdentity.cpp in
    // libsecurity_keychain, specifically
    // _SecIdentityCopyPreferenceMatchingName().
    {
      base::AutoLock lock(crypto::GetMacSecurityServicesLock());
      preferred_sec_identity.reset(
          SecIdentityCopyPreferred(domain_str.get(), nullptr, nullptr));
    }
  }

  // Now enumerate the identities in the available keychains.
  std::unique_ptr<ClientCertIdentityMac> preferred_identity;
  ClientCertIdentityMacList regular_identities;

// TODO(crbug.com/40233280): Is it still true, as claimed below, that
// SecIdentitySearchCopyNext sometimes returns identities missed by
// SecItemCopyMatching? Add some histograms to test this and, if none are
// missing, remove this code.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  SecIdentitySearchRef search = nullptr;
  OSStatus err;
  {
    base::AutoLock lock(crypto::GetMacSecurityServicesLock());
    err = SecIdentitySearchCreate(nullptr, CSSM_KEYUSE_SIGN, &search);
  }
  if (err)
    return ClientCertIdentityList();
  ScopedCFTypeRef<SecIdentitySearchRef> scoped_search(search);
  while (!err) {
    ScopedCFTypeRef<SecIdentityRef> sec_identity;
    {
      base::AutoLock lock(crypto::GetMacSecurityServicesLock());
      err = SecIdentitySearchCopyNext(search, sec_identity.InitializeInto());
    }
    if (err)
      break;
    AddIdentity(std::move(sec_identity), preferred_sec_identity.get(),
                &regular_identities, &preferred_identity);
  }

  if (err != errSecItemNotFound) {
    OSSTATUS_LOG(ERROR, err) << "SecIdentitySearch error";
    return ClientCertIdentityList();
  }
#pragma clang diagnostic pop  // "-Wdeprecated-declarations"

  // macOS provides two ways to search for identities. SecIdentitySearchCreate()
  // is deprecated, as it relies on CSSM_KEYUSE_SIGN (part of the deprecated
  // CDSM/CSSA implementation), but is necessary to return some certificates
  // that would otherwise not be returned by SecItemCopyMatching(), which is the
  // non-deprecated way. However, SecIdentitySearchCreate() will not return all
  // items, particularly smart-card based identities, so it's necessary to call
  // both functions.
  static const void* kKeys[] = {
      kSecClass, kSecMatchLimit, kSecReturnRef, kSecAttrCanSign,
  };
  static const void* kValues[] = {
      kSecClassIdentity, kSecMatchLimitAll, kCFBooleanTrue, kCFBooleanTrue,
  };
  ScopedCFTypeRef<CFDictionaryRef> query(CFDictionaryCreate(
      kCFAllocatorDefault, kKeys, kValues, std::size(kValues),
      &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
  ScopedCFTypeRef<CFArrayRef> result;
  {
    base::AutoLock lock(crypto::GetMacSecurityServicesLock());
    err = SecItemCopyMatching(
        query.get(), reinterpret_cast<CFTypeRef*>(result.InitializeInto()));
  }
  if (!err) {
    for (CFIndex i = 0; i < CFArrayGetCount(result.get()); i++) {
      SecIdentityRef item = reinterpret_cast<SecIdentityRef>(
          const_cast<void*>(CFArrayGetValueAtIndex(result.get(), i)));
      AddIdentity(
          ScopedCFTypeRef<SecIdentityRef>(item, base::scoped_policy::RETAIN),
          preferred_sec_identity.get(), &regular_identities,
          &preferred_identity);
    }
  }

  ClientCertIdentityList selected_identities;
  GetClientCertsImpl(std::move(preferred_identity),
                     std::move(regular_identities), *request, true,
                     &selected_identities);
  return selected_identities;
}

}  // namespace

ClientCertStoreMac::ClientCertStoreMac() = default;

ClientCertStoreMac::~ClientCertStoreMac() = default;

void ClientCertStoreMac::GetClientCerts(
    scoped_refptr<const SSLCertRequestInfo> request,
    ClientCertListCallback callback) {
  GetSSLPlatformKeyTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetClientCertsOnBackgroundThread, std::move(request)),
      base::BindOnce(&ClientCertStoreMac::OnClientCertsResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ClientCertStoreMac::OnClientCertsResponse(
    ClientCertListCallback callback,
    ClientCertIdentityList identities) {
  std::move(callback).Run(std::move(identities));
}

bool ClientCertStoreMac::SelectClientCertsForTesting(
    ClientCertIdentityMacList input_identities,
    const SSLCertRequestInfo& request,
    ClientCertIdentityList* selected_identities) {
  GetClientCertsImpl(nullptr, std::move(input_identities), request, false,
                     selected_identities);
  return true;
}

bool ClientCertStoreMac::SelectClientCertsGivenPreferredForTesting(
    std::unique_ptr<ClientCertIdentityMac> preferred_identity,
    ClientCertIdentityMacList regular_identities,
    const SSLCertRequestInfo& request,
    ClientCertIdentityList* selected_identities) {
  GetClientCertsImpl(std::move(preferred_identity),
                     std::move(regular_identities), request, false,
                     selected_identities);
  return true;
}

}  // namespace net
