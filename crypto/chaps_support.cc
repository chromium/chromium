// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/chaps_support.h"

#include <dlfcn.h>
#include <secmod.h>
#include <secmodt.h>

#include <string_view>

#include "base/logging.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/stack_allocated.h"
#include "base/threading/scoped_blocking_call.h"
#include "crypto/scoped_nss_types.h"
#include "nss_util_internal.h"

namespace crypto {

namespace {

// Constants for loading the Chrome OS TPM-backed PKCS #11 library.
const char kChapsModuleName[] = "Chaps";
const char kChapsPath[] = "libchaps.so";

class ScopedChapsLoadFixup {
  STACK_ALLOCATED();

 public:
  ScopedChapsLoadFixup();
  ~ScopedChapsLoadFixup();

 private:
#if defined(COMPONENT_BUILD)
  // This field stores a handle and is not a pointer to PA memory.
  // Also, this class is always stack-allocated and visibility is limited.
  // Hence no benefit from using raw_ptr<void>.
  RAW_PTR_EXCLUSION void* chaps_handle_;
#endif
};

#if defined(COMPONENT_BUILD)

ScopedChapsLoadFixup::ScopedChapsLoadFixup() {
  // HACK: libchaps links the system protobuf and there are symbol conflicts
  // with the bundled copy. Load chaps with RTLD_DEEPBIND to workaround.
  chaps_handle_ = dlopen(kChapsPath, RTLD_LOCAL | RTLD_NOW | RTLD_DEEPBIND);
}

ScopedChapsLoadFixup::~ScopedChapsLoadFixup() {
  // LoadNSSModule() will have taken a 2nd reference.
  if (chaps_handle_)
    dlclose(chaps_handle_);
}

#else

ScopedChapsLoadFixup::ScopedChapsLoadFixup() = default;
ScopedChapsLoadFixup::~ScopedChapsLoadFixup() = default;

#endif  // defined(COMPONENT_BUILD)

}  // namespace

SECMODModule* LoadChaps() {
  // NSS functions may reenter //net via extension hooks. If the reentered
  // code needs to synchronously wait for a task to run but the thread pool in
  // which that task must run doesn't have enough threads to schedule it, a
  // deadlock occurs. To prevent that, the base::ScopedBlockingCall below
  // increments the thread pool capacity for the duration of the TPM
  // initialization.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  ScopedChapsLoadFixup chaps_loader;

  DVLOG(3) << "Loading chaps...";
  return LoadNSSModule(
      kChapsModuleName, kChapsPath,
      // For more details on these parameters, see:
      // https://developer.mozilla.org/en/PKCS11_Module_Specs
      // slotFlags=[PublicCerts] -- Certificates and public keys can be
      //   read from this slot without requiring a call to C_Login.
      // askpw=only -- Only authenticate to the token when necessary.
      "NSS=\"slotParams=(0={slotFlags=[PublicCerts] askpw=only})\"");
}

ScopedPK11Slot GetChapsSlot(SECMODModule* chaps_module, CK_SLOT_ID slot_id) {
  DCHECK(chaps_module);

  // NSS functions may reenter //net via extension hooks. If the reentered
  // code needs to synchronously wait for a task to run but the thread pool in
  // which that task must run doesn't have enough threads to schedule it, a
  // deadlock occurs. To prevent that, the base::ScopedBlockingCall below
  // increments the thread pool capacity for the duration of the TPM
  // initialization.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  DVLOG(3) << "Poking chaps module.";
  SECStatus rv = SECMOD_UpdateSlotList(chaps_module);
  if (rv != SECSuccess)
    LOG(ERROR) << "SECMOD_UpdateSlotList failed: " << PORT_GetError();

  ScopedPK11Slot slot =
      ScopedPK11Slot(SECMOD_LookupSlot(chaps_module->moduleID, slot_id));
  if (!slot)
    LOG(ERROR) << "TPM slot " << slot_id << " not found.";
  return slot;
}

bool IsChapsModule(SECMODModule* pk11_module) {
  return pk11_module && std::string_view(pk11_module->commonName) ==
                            std::string_view(kChapsModuleName);
}

bool IsSlotProvidedByChaps(PK11SlotInfo* slot) {
  return slot && IsChapsModule(PK11_GetModule(slot));
}

}  // namespace crypto
