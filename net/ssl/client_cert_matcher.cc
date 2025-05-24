// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/client_cert_matcher.h"

#include <algorithm>

#include "base/containers/span.h"
#include "base/logging.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

namespace {

bool MatchClientCertificateIssuers(
    X509Certificate* cert,
    const std::vector<std::string>& cert_authorities,
    const ClientCertIssuerSourceCollection& sources,
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>>* intermediates) {
  constexpr size_t kMaxDepth = 20;
  intermediates->clear();

  // If the request didn't supply `cert_authorities`, all client certs are
  // returned.
  if (cert_authorities.empty()) {
    return true;
  }

  base::span<const uint8_t> current_issuer;
  base::span<const uint8_t> current_subject;
  if (!asn1::ExtractIssuerAndSubjectFromDERCert(
          cert->cert_span(), &current_issuer, &current_subject)) {
    return false;
  }

  while (intermediates->size() < kMaxDepth) {
    // If the current cert in the chain is issued by one of the names in
    // `cert_authorities`, this chain matches the request.
    for (const std::string& authority : cert_authorities) {
      if (base::as_byte_span(authority) == current_issuer) {
        return true;
      }
    }

    // If the chain reached a self-issued cert before matching the requested
    // `cert_authorities`, give up.
    if (current_issuer == current_subject) {
      return false;
    }

    // Look for an issuer of the current cert.
    bool found_issuer = false;
    for (const auto& source : sources) {
      std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> issuers =
          source->GetCertsByName(current_issuer);
      for (auto& issuer : issuers) {
        if (asn1::ExtractIssuerAndSubjectFromDERCert(
                x509_util::CryptoBufferAsSpan(issuer.get()), &current_issuer,
                &current_subject)) {
          // The first issuer found at each step is used. This algorithm doesn't
          // do a full graph exploration.
          found_issuer = true;
          intermediates->push_back(std::move(issuer));
          break;
        }
      }
    }

    if (!found_issuer) {
      // No issuers were found, give up.
      return false;
    }
  }

  return false;
}

}  // namespace

ClientCertIssuerSourceInMemory::ClientCertIssuerSourceInMemory(
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certs)
    : certs_(std::move(certs)) {
  for (const auto& cert : certs_) {
    std::string_view subject;
    if (asn1::ExtractSubjectFromDERCert(
            x509_util::CryptoBufferAsStringPiece(cert.get()), &subject)) {
      cert_map_.emplace(base::as_byte_span(subject), cert.get());
    }
  }
}

ClientCertIssuerSourceInMemory::~ClientCertIssuerSourceInMemory() = default;

std::vector<bssl::UniquePtr<CRYPTO_BUFFER>>
ClientCertIssuerSourceInMemory::GetCertsByName(base::span<const uint8_t> name) {
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> result;
  auto range = cert_map_.equal_range(name);
  for (auto it = range.first; it != range.second; ++it) {
    result.push_back(bssl::UpRef(it->second));
  }
  return result;
}

void FilterMatchingClientCertIdentities(
    ClientCertIdentityList* identities,
    const SSLCertRequestInfo& request,
    const ClientCertIssuerSourceCollection& sources) {
  size_t num_raw = 0;

  auto keep_iter = identities->begin();

  base::Time now = base::Time::Now();

  for (auto examine_iter = identities->begin();
       examine_iter != identities->end(); ++examine_iter) {
    ++num_raw;

    X509Certificate* cert = (*examine_iter)->certificate();

    // Only offer unexpired certificates.
    // TODO(https://crbug.com/379943126): If the client system time is
    // incorrect this may prune certificates that the server would have
    // accepted (and we may still successfully validate the server certificate
    // by using secure time). Consider removing.
    if (now < cert->valid_start()) {
      DVLOG(2) << "is not yet valid: " << cert->subject().GetDisplayName();
      continue;
    }
    if (now > cert->valid_expiry()) {
      DVLOG(2) << "is expired: " << cert->subject().GetDisplayName();
      continue;
    }

    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
    if (!MatchClientCertificateIssuers(cert, request.cert_authorities, sources,
                                       &intermediates)) {
      DVLOG(2) << "doesn't match: " << cert->subject().GetDisplayName();
      continue;
    } else {
      DVLOG(2) << "found a match: " << cert->subject().GetDisplayName();
    }

    // Retain a copy of the intermediates. Some deployments expect the client to
    // supply intermediates out of the local store. See
    // https://crbug.com/548631.
    (*examine_iter)->SetIntermediates(std::move(intermediates));

    if (examine_iter == keep_iter) {
      ++keep_iter;
    } else {
      *keep_iter++ = std::move(*examine_iter);
    }
  }
  identities->erase(keep_iter, identities->end());

  DVLOG(2) << "num_raw:" << num_raw << " num_filtered:" << identities->size();

  std::ranges::sort(*identities, ClientCertIdentitySorter());
}

}  // namespace net
