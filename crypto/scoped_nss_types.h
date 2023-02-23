// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_SCOPED_NSS_TYPES_H_
#define CRYPTO_SCOPED_NSS_TYPES_H_

#include <cert.h>
#include <certt.h>
#include <keyhi.h>
#include <nss.h>
#include <pk11pub.h>
#include <plarena.h>

#include <memory>

namespace crypto {

template <typename Type, void (*Destroyer)(Type*)>
struct NSSDestroyer {
  void operator()(Type* ptr) const { Destroyer(ptr); }
};

template <typename Type, void (*Destroyer)(Type*, PRBool), PRBool freeit>
struct NSSDestroyer1 {
  void operator()(Type* ptr) const { Destroyer(ptr, freeit); }
};

// Define some convenient scopers around NSS pointers.
typedef std::unique_ptr<
    PK11Context,
    NSSDestroyer1<PK11Context, PK11_DestroyContext, PR_TRUE>>
    ScopedPK11Context;
typedef std::unique_ptr<PK11SlotInfo, NSSDestroyer<PK11SlotInfo, PK11_FreeSlot>>
    ScopedPK11Slot;
typedef std::unique_ptr<PK11SlotList,
                        NSSDestroyer<PK11SlotList, PK11_FreeSlotList>>
    ScopedPK11SlotList;
typedef std::unique_ptr<
    SECKEYPublicKeyList,
    NSSDestroyer<SECKEYPublicKeyList, SECKEY_DestroyPublicKeyList>>
    ScopedSECKEYPublicKeyList;
typedef std::unique_ptr<PK11SymKey, NSSDestroyer<PK11SymKey, PK11_FreeSymKey>>
    ScopedPK11SymKey;
typedef std::unique_ptr<SECKEYPublicKey,
                        NSSDestroyer<SECKEYPublicKey, SECKEY_DestroyPublicKey>>
    ScopedSECKEYPublicKey;
typedef std::unique_ptr<
    SECKEYPrivateKey,
    NSSDestroyer<SECKEYPrivateKey, SECKEY_DestroyPrivateKey>>
    ScopedSECKEYPrivateKey;
typedef std::unique_ptr<
    SECAlgorithmID,
    NSSDestroyer1<SECAlgorithmID, SECOID_DestroyAlgorithmID, PR_TRUE>>
    ScopedSECAlgorithmID;
typedef std::unique_ptr<SECItem,
                        NSSDestroyer1<SECItem, SECITEM_FreeItem, PR_TRUE>>
    ScopedSECItem;
typedef std::unique_ptr<PLArenaPool,
                        NSSDestroyer1<PLArenaPool, PORT_FreeArena, PR_FALSE>>
    ScopedPLArenaPool;
typedef std::unique_ptr<
    CERTSubjectPublicKeyInfo,
    NSSDestroyer<CERTSubjectPublicKeyInfo, SECKEY_DestroySubjectPublicKeyInfo>>
    ScopedCERTSubjectPublicKeyInfo;
typedef std::unique_ptr<CERTCertList,
                        NSSDestroyer<CERTCertList, CERT_DestroyCertList>>
    ScopedCERTCertList;

}  // namespace crypto

#endif  // CRYPTO_SCOPED_NSS_TYPES_H_
