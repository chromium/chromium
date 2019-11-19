// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/client_cert_store_mac.h"

#include <CommonCrypto/CommonDigest.h>
#include <CoreFoundation/CFArray.h>
#include <CoreServices/CoreServices.h>
#include <Security/SecBase.h>
#include <Security/Security.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task_runner_util.h"
#include "crypto/mac_security_services_lock.h"
#include "net/base/host_port_pair.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_ios_and_mac.h"
#include "net/cert/x509_util_mac.h"
#include "net/ssl/client_cert_identity_mac.h"
#include "net/ssl/ssl_platform_key_util.h"

using base::ScopedCFTypeRef;

namespace net {

// CSSM functions are deprecated as of OSX 10.7, but have no replacement.
// https://bugs.chromium.org/p/chromium/issues/detail?id=590914#c1
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace {

// Gets the issuer for a given cert, starting with the cert itself and
// including the intermediate and finally root certificates (if any).
// This function calls SecTrust but doesn't actually pay attention to the trust
// result: it shouldn't be used to determine trust, just to traverse the chain.
// Caller is responsible for releasing the value stored into *out_cert_chain.
OSStatus CopyCertChain(SecCertificateRef cert_handle,
                       CFArrayRef* out_cert_chain) {
  DCHECK(cert_handle);
  DCHECK(out_cert_chain);

  // Create an SSL policy ref configured for client cert evaluation.
  SecPolicyRef ssl_policy;
  OSStatus result = x509_util::CreateSSLClientPolicy(&ssl_policy);
  if (result)
    return result;
  ScopedCFTypeRef<SecPolicyRef> scoped_ssl_policy(ssl_policy);

  // Create a SecTrustRef.
  ScopedCFTypeRef<CFArrayRef> input_certs(CFArrayCreate(
      NULL, const_cast<const void**>(reinterpret_cast<void**>(&cert_handle)),
      1, &kCFTypeArrayCallBacks));
  SecTrustRef trust_ref = NULL;
  {
    base::AutoLock lock(crypto::GetMacSecurityServicesLock());
    result = SecTrustCreateWithCertificates(input_certs, ssl_policy,
                                            &trust_ref);
  }
  if (result)
    return result;
  ScopedCFTypeRef<SecTrustRef> trust(trust_ref);

  // Evaluate trust, which creates the cert chain.
  SecTrustResultType status;
  CSSM_TP_APPLE_EVIDENCE_INFO* status_chain;
  {
    base::AutoLock lock(crypto::GetMacSecurityServicesLock());
    result = SecTrustEvaluate(trust, &status);
  }
  if (result)
    return result;
  {
    base::AutoLock lock(crypto::GetMacSecurityServicesLock());
    result = SecTrustGetResult(trust, &status, out_cert_chain, &status_chain);
  }
  return result;
}

// Returns true if |*identity| is issued by an authority in |valid_issuers|
// according to Keychain Services, rather than using |identity|'s intermediate
// certificates. If it is, |*identity| is updated to include the intermediates.
bool IsIssuedByInKeychain(const std::vector<std::string>& valid_issuers,
                          ClientCertIdentity* identity) {
  DCHECK(identity);
  DCHECK(identity->sec_identity_ref());

  ScopedCFTypeRef<SecCertificateRef> os_cert;
  int err = SecIdentityCopyCertificate(identity->sec_identity_ref(),
                                       os_cert.InitializeInto());
  if (err != noErr)
    return false;
  CFArrayRef cert_chain = NULL;
  OSStatus result = CopyCertChain(os_cert.get(), &cert_chain);
  if (result) {
    OSSTATUS_LOG(ERROR, result) << "CopyCertChain error";
    return false;
  }

  if (!cert_chain)
    return false;

  std::vector<SecCertificateRef> intermediates;
  for (CFIndex i = 1, chain_count = CFArrayGetCount(cert_chain);
       i < chain_count; ++i) {
    SecCertificateRef sec_cert = reinterpret_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(cert_chain, i)));
    intermediates.push_back(sec_cert);
  }

  // Allow UTF-8 inside PrintableStrings in client certificates. See
  // crbug.com/770323.
  X509Certificate::UnsafeCreateOptions options;
  options.printable_string_is_utf8 = true;
  scoped_refptr<X509Certificate> new_cert(
      x509_util::CreateX509CertificateFromSecCertificate(
          os_cert.get(), intermediates, options));
  CFRelease(cert_chain);  // Also frees |intermediates|.

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

// Returns true if |purpose| is listed as allowed in |usage|. This
// function also considers the "Any" purpose. If the attribute is
// present and empty, we return false.
bool ExtendedKeyUsageAllows(const CE_ExtendedKeyUsage* usage,
                            const CSSM_OID* purpose) {
  for (unsigned p = 0; p < usage->numPurposes; ++p) {
    if (x509_util::CSSMOIDEqual(&usage->purposes[p], purpose))
      return true;
    if (x509_util::CSSMOIDEqual(&usage->purposes[p],
                                &CSSMOID_ExtendedKeyUsageAny))
      return true;
  }
  return false;
}

// Does |cert|'s usage allow SSL client authentication?
bool SupportsSSLClientAuth(SecCertificateRef cert) {
  x509_util::CSSMCachedCertificate cached_cert;
  OSStatus status = cached_cert.Init(cert);
  if (status)
    return false;

  // RFC5280 says to take the intersection of the two extensions.
  //
  // Our underlying crypto libraries don't expose
  // ClientCertificateType, so for now we will not support fixed
  // Diffie-Hellman mechanisms. For rsa_sign, we need the
  // digitalSignature bit.
  //
  // In particular, if a key has the nonRepudiation bit and not the
  // digitalSignature one, we will not offer it to the user.
  x509_util::CSSMFieldValue key_usage;
  status = cached_cert.GetField(&CSSMOID_KeyUsage, &key_usage);
  if (status == CSSM_OK && key_usage.field()) {
    const CSSM_X509_EXTENSION* ext = key_usage.GetAs<CSSM_X509_EXTENSION>();
    const CE_KeyUsage* key_usage_value =
        reinterpret_cast<const CE_KeyUsage*>(ext->value.parsedValue);
    if (!((*key_usage_value) & CE_KU_DigitalSignature))
      return false;
  }

  status = cached_cert.GetField(&CSSMOID_ExtendedKeyUsage, &key_usage);
  if (status == CSSM_OK && key_usage.field()) {
    const CSSM_X509_EXTENSION* ext = key_usage.GetAs<CSSM_X509_EXTENSION>();
    const CE_ExtendedKeyUsage* ext_key_usage =
        reinterpret_cast<const CE_ExtendedKeyUsage*>(ext->value.parsedValue);
    if (!ExtendedKeyUsageAllows(ext_key_usage, &CSSMOID_ClientAuth))
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
void GetClientCertsImpl(std::unique_ptr<ClientCertIdentity> preferred_identity,
                        ClientCertIdentityList regular_identities,
                        const SSLCertRequestInfo& request,
                        bool query_keychain,
                        ClientCertIdentityList* selected_identities) {
  scoped_refptr<X509Certificate> preferred_cert_orig;
  ClientCertIdentityList preliminary_list(std::move(regular_identities));
  if (preferred_identity) {
    preferred_cert_orig = preferred_identity->certificate();
    preliminary_list.insert(preliminary_list.begin(),
                            std::move(preferred_identity));
  }

  selected_identities->clear();
  for (size_t i = 0; i < preliminary_list.size(); ++i) {
    std::unique_ptr<ClientCertIdentity>& cert = preliminary_list[i];
    if (cert->certificate()->HasExpired())
      continue;

    // Skip duplicates (a cert may be in multiple keychains).
    auto cert_iter = std::find_if(
        selected_identities->begin(), selected_identities->end(),
        [&cert](
            const std::unique_ptr<ClientCertIdentity>& other_cert_identity) {
          return x509_util::CryptoBufferEqual(
              cert->certificate()->cert_buffer(),
              other_cert_identity->certificate()->cert_buffer());
        });
    if (cert_iter != selected_identities->end())
      continue;

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
                 ClientCertIdentityList* regular_identities,
                 std::unique_ptr<ClientCertIdentity>* preferred_identity) {
  OSStatus err;
  ScopedCFTypeRef<SecCertificateRef> cert_handle;
  err = SecIdentityCopyCertificate(sec_identity.get(),
                                   cert_handle.InitializeInto());
  if (err != noErr)
    return;

  if (!SupportsSSLClientAuth(cert_handle.get()))
    return;

  // Allow UTF-8 inside PrintableStrings in client certificates. See
  // crbug.com/770323.
  X509Certificate::UnsafeCreateOptions options;
  options.printable_string_is_utf8 = true;
  scoped_refptr<X509Certificate> cert(
      x509_util::CreateX509CertificateFromSecCertificate(cert_handle.get(), {},
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
    const SSLCertRequestInfo& request) {
  std::string server_domain = request.host_and_port.host();

  ScopedCFTypeRef<SecIdentityRef> preferred_sec_identity;
  if (!server_domain.empty()) {
    // See if there's an identity preference for this domain:
    ScopedCFTypeRef<CFStringRef> domain_str(
        base::SysUTF8ToCFStringRef("https://" + server_domain));
    SecIdentityRef sec_identity = NULL;
    // While SecIdentityCopyPreferences appears to take a list of CA issuers
    // to restrict the identity search to, within Security.framework the
    // argument is ignored and filtering unimplemented. See
    // SecIdentity.cpp in libsecurity_keychain, specifically
    // _SecIdentityCopyPreferenceMatchingName().
    {
      base::AutoLock lock(crypto::GetMacSecurityServicesLock());
      if (SecIdentityCopyPreference(domain_str, 0, NULL, &sec_identity) ==
          noErr)
        preferred_sec_identity.reset(sec_identity);
    }
  }

  // Now enumerate the identities in the available keychains.
  std::unique_ptr<ClientCertIdentity> preferred_identity;
  ClientCertIdentityList regular_identities;

  SecIdentitySearchRef search = NULL;
  OSStatus err;
  {
    base::AutoLock lock(crypto::GetMacSecurityServicesLock());
    err = SecIdentitySearchCreate(NULL, CSSM_KEYUSE_SIGN, &search);
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
      kCFAllocatorDefault, kKeys, kValues, base::size(kValues),
      &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
  ScopedCFTypeRef<CFArrayRef> result;
  {
    base::AutoLock lock(crypto::GetMacSecurityServicesLock());
    err = SecItemCopyMatching(
        query, reinterpret_cast<CFTypeRef*>(result.InitializeInto()));
  }
  if (!err) {
    for (CFIndex i = 0; i < CFArrayGetCount(result); i++) {
      SecIdentityRef item = reinterpret_cast<SecIdentityRef>(
          const_cast<void*>(CFArrayGetValueAtIndex(result, i)));
      AddIdentity(
          ScopedCFTypeRef<SecIdentityRef>(item, base::scoped_policy::RETAIN),
          preferred_sec_identity.get(), &regular_identities,
          &preferred_identity);
    }
  }

  ClientCertIdentityList selected_identities;
  GetClientCertsImpl(std::move(preferred_identity),
                     std::move(regular_identities), request, true,
                     &selected_identities);
  return selected_identities;
}

}  // namespace

ClientCertStoreMac::ClientCertStoreMac() {}

ClientCertStoreMac::~ClientCertStoreMac() {}

void ClientCertStoreMac::GetClientCerts(const SSLCertRequestInfo& request,
                                        ClientCertListCallback callback) {
  base::PostTaskAndReplyWithResult(
      GetSSLPlatformKeyTaskRunner().get(), FROM_HERE,
      // Caller is responsible for keeping the |request| alive
      // until the callback is run, so std::cref is safe.
      base::BindOnce(&GetClientCertsOnBackgroundThread, std::cref(request)),
      std::move(callback));
}

bool ClientCertStoreMac::SelectClientCertsForTesting(
    ClientCertIdentityList input_identities,
    const SSLCertRequestInfo& request,
    ClientCertIdentityList* selected_identities) {
  GetClientCertsImpl(NULL, std::move(input_identities), request, false,
                     selected_identities);
  return true;
}

bool ClientCertStoreMac::SelectClientCertsGivenPreferredForTesting(
    std::unique_ptr<ClientCertIdentity> preferred_identity,
    ClientCertIdentityList regular_identities,
    const SSLCertRequestInfo& request,
    ClientCertIdentityList* selected_identities) {
  GetClientCertsImpl(std::move(preferred_identity),
                     std::move(regular_identities), request, false,
                     selected_identities);
  return true;
}

#pragma clang diagnostic pop  // "-Wdeprecated-declarations"

}  // namespace net
