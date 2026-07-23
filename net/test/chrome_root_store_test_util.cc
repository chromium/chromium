// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/chrome_root_store_test_util.h"

#include "net/cert/root_store_proto_lite/signer_set.pb.h"
#include "net/cert/x509_util.h"

namespace net {

namespace {

chrome_root_store::Signer* FillSignerSetSigner(
    chrome_root_store::Signer* signer,
    base::span<const uint8_t> log_id,
    std::string_view operator_name) {
  signer->set_friendly_name(x509_util::RelativeOidToString(log_id));
  signer->set_base_id(x509_util::RelativeOidToString(log_id));
  // Signer will be returned with a fake key, which is good enough for most
  // tests since the key isn't parsed until doing a signature verification.
  // Tests that actually need to test signature verification can set the key
  // field on the returned object to their chosen key.
  signer->set_key("fakekey");
  signer->set_realm(chrome_root_store::REALM_PUBLICLY_TRUSTED);
  signer->set_signature_algorithm(
      chrome_root_store::SIGNATURE_ALGORITHM_ML_DSA44);

  auto* state = signer->add_state_history();
  state->set_state(chrome_root_store::STATE_USABLE);
  state->mutable_state_start()->set_seconds(1);

  auto* operator_entry = signer->add_operator_history();
  operator_entry->set_name(operator_name);
  operator_entry->mutable_operator_start()->set_seconds(1);

  return signer;
}

}  // namespace

chrome_root_store::Signer* AddSignerSetIssuer(
    chrome_root_store::SignerSet& signer_set,
    base::span<const uint8_t> log_id,
    std::string_view operator_name,
    std::optional<int32_t> crs_root_id) {
  auto* issuer =
      FillSignerSetSigner(signer_set.add_issuers(), log_id, operator_name);
  issuer->set_type(chrome_root_store::SIGNER_TYPE_ISSUER);
  if (crs_root_id) {
    issuer->set_crs_root_id(*crs_root_id);
  }

  return issuer;
}

chrome_root_store::Signer* AddSignerSetMirror(
    chrome_root_store::SignerSet& signer_set,
    base::span<const uint8_t> log_id,
    std::string_view operator_name) {
  auto* mirror =
      FillSignerSetSigner(signer_set.add_mirrors(), log_id, operator_name);
  mirror->set_type(chrome_root_store::SIGNER_TYPE_MIRROR);
  return mirror;
}

}  // namespace net
