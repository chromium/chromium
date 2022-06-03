// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/nss_key_util.h"

#include <cryptohi.h>
#include <keyhi.h>
#include <pk11pub.h>
#include <secmod.h>
#include <stdint.h>

#include <memory>

#include "base/logging.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"

namespace crypto {

namespace {

// Decodes |input| as a SubjectPublicKeyInfo and returns a SECItem containing
// the CKA_ID of that public key or nullptr on error.
ScopedSECItem MakeIDFromSPKI(base::span<const uint8_t> input) {
  ScopedCERTSubjectPublicKeyInfo spki = DecodeSubjectPublicKeyInfoNSS(input);
  if (!spki)
    return nullptr;

  ScopedSECKEYPublicKey result(SECKEY_ExtractPublicKey(spki.get()));
  if (!result)
    return nullptr;

  // See pk11_MakeIDFromPublicKey from NSS. For now, only RSA and EC keys are
  // supported.
  if (SECKEY_GetPublicKeyType(result.get()) == rsaKey)
    return ScopedSECItem(PK11_MakeIDFromPubKey(&result->u.rsa.modulus));
  if (SECKEY_GetPublicKeyType(result.get()) == ecKey)
    return ScopedSECItem(PK11_MakeIDFromPubKey(&result->u.ec.publicValue));
  return nullptr;
}

}  // namespace

bool GenerateRSAKeyPairNSS(PK11SlotInfo* slot,
                           uint16_t num_bits,
                           bool permanent,
                           ScopedSECKEYPublicKey* public_key,
                           ScopedSECKEYPrivateKey* private_key) {
  DCHECK(slot);

  PK11RSAGenParams param;
  param.keySizeInBits = num_bits;
  param.pe = 65537L;
  SECKEYPublicKey* public_key_raw = nullptr;
  private_key->reset(PK11_GenerateKeyPair(slot, CKM_RSA_PKCS_KEY_PAIR_GEN,
                                          &param, &public_key_raw, permanent,
                                          permanent /* sensitive */, nullptr));
  if (!*private_key)
    return false;

  public_key->reset(public_key_raw);
  return true;
}

bool GenerateECKeyPairNSS(PK11SlotInfo* slot,
                          const SECOidTag named_curve,
                          bool permanent,
                          ScopedSECKEYPublicKey* public_key,
                          ScopedSECKEYPrivateKey* private_key) {
  DCHECK(slot);

  if (named_curve != SEC_OID_ANSIX962_EC_PRIME256V1) {
    LOG(ERROR) << "SECOidTag: " << named_curve
               << " is not supported. Only SEC_OID_ANSIX962_EC_PRIME256V1 is "
                  "supported for elliptic curve key pair generation.";
    return false;
  }

  SECOidData* oid_data = SECOID_FindOIDByTag(named_curve);
  if (!oid_data) {
    LOG(ERROR) << "SECOID_FindOIDByTag: " << PORT_GetError();
    return false;
  }

  std::vector<uint8_t> parameters_buf(2 + oid_data->oid.len);
  SECKEYECParams ec_parameters = {siDEROID, parameters_buf.data(),
                                  static_cast<unsigned>(parameters_buf.size())};

  ec_parameters.data[0] = SEC_ASN1_OBJECT_ID;
  ec_parameters.data[1] = oid_data->oid.len;
  memcpy(ec_parameters.data + 2, oid_data->oid.data, oid_data->oid.len);
  SECKEYPublicKey* public_key_raw = nullptr;
  private_key->reset(PK11_GenerateKeyPair(slot, CKM_EC_KEY_PAIR_GEN,
                                          &ec_parameters, &public_key_raw,
                                          permanent, permanent, nullptr));
  if (!*private_key)
    return false;

  public_key->reset(public_key_raw);
  return true;
}

ScopedSECKEYPrivateKey ImportNSSKeyFromPrivateKeyInfo(
    PK11SlotInfo* slot,
    const std::vector<uint8_t>& input,
    bool permanent) {
  DCHECK(slot);

  ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  DCHECK(arena);

  // Excess data is illegal, but NSS silently accepts it, so first ensure that
  // |input| consists of a single ASN.1 element.
  SECItem input_item;
  input_item.data = const_cast<unsigned char*>(input.data());
  input_item.len = input.size();
  SECItem der_private_key_info;
  SECStatus rv =
      SEC_QuickDERDecodeItem(arena.get(), &der_private_key_info,
                             SEC_ASN1_GET(SEC_AnyTemplate), &input_item);
  if (rv != SECSuccess)
    return nullptr;

  // Allow the private key to be used for key unwrapping, data decryption,
  // and signature generation.
  const unsigned int key_usage =
      KU_KEY_ENCIPHERMENT | KU_DATA_ENCIPHERMENT | KU_DIGITAL_SIGNATURE;
  SECKEYPrivateKey* key_raw = nullptr;
  rv = PK11_ImportDERPrivateKeyInfoAndReturnKey(
      slot, &der_private_key_info, nullptr, nullptr, permanent,
      permanent /* sensitive */, key_usage, &key_raw, nullptr);
  if (rv != SECSuccess)
    return nullptr;
  return ScopedSECKEYPrivateKey(key_raw);
}

ScopedSECKEYPrivateKey FindNSSKeyFromPublicKeyInfo(
    base::span<const uint8_t> input) {
  EnsureNSSInit();

  ScopedSECItem cka_id(MakeIDFromSPKI(input));
  if (!cka_id)
    return nullptr;

  // Search all slots in all modules for the key with the given ID.
  AutoSECMODListReadLock auto_lock;
  const SECMODModuleList* head = SECMOD_GetDefaultModuleList();
  for (const SECMODModuleList* item = head; item != nullptr;
       item = item->next) {
    int slot_count = item->module->loaded ? item->module->slotCount : 0;
    for (int i = 0; i < slot_count; i++) {
      // Look for the key in slot |i|.
      ScopedSECKEYPrivateKey key(
          PK11_FindKeyByKeyID(item->module->slots[i], cka_id.get(), nullptr));
      if (key)
        return key;
    }
  }

  // The key wasn't found in any module.
  return nullptr;
}

ScopedSECKEYPrivateKey FindNSSKeyFromPublicKeyInfoInSlot(
    base::span<const uint8_t> input,
    PK11SlotInfo* slot) {
  DCHECK(slot);

  ScopedSECItem cka_id(MakeIDFromSPKI(input));
  if (!cka_id)
    return nullptr;

  return ScopedSECKEYPrivateKey(
      PK11_FindKeyByKeyID(slot, cka_id.get(), nullptr));
}

ScopedCERTSubjectPublicKeyInfo DecodeSubjectPublicKeyInfoNSS(
    base::span<const uint8_t> input) {
  // First, decode and save the public key.
  SECItem key_der;
  key_der.type = siBuffer;
  key_der.data = const_cast<unsigned char*>(input.data());
  key_der.len = input.size();

  ScopedCERTSubjectPublicKeyInfo spki(
      SECKEY_DecodeDERSubjectPublicKeyInfo(&key_der));
  return spki;
}

}  // namespace crypto
