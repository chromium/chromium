// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/nss_profile_filter_chromeos.h"

#include <memory>
#include <utility>

#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "net/cert/x509_certificate.h"

namespace net {

namespace {

std::string CertSlotsString(CERTCertificate* cert) {
  std::string result;
  crypto::ScopedPK11SlotList slots_for_cert(
      PK11_GetAllSlotsForCert(cert, NULL));
  for (PK11SlotListElement* slot_element =
           PK11_GetFirstSafe(slots_for_cert.get());
       slot_element;
       slot_element =
           PK11_GetNextSafe(slots_for_cert.get(), slot_element, PR_FALSE)) {
    if (!result.empty())
      result += ',';
    base::StringAppendF(&result,
                        "%lu:%lu",
                        PK11_GetModuleID(slot_element->slot),
                        PK11_GetSlotID(slot_element->slot));
  }
  return result;
}

}  // namespace

NSSProfileFilterChromeOS::NSSProfileFilterChromeOS() = default;

NSSProfileFilterChromeOS::NSSProfileFilterChromeOS(
    const NSSProfileFilterChromeOS& other) {
  public_slot_.reset(other.public_slot_ ?
      PK11_ReferenceSlot(other.public_slot_.get()) :
      NULL);
  private_slot_.reset(other.private_slot_ ?
      PK11_ReferenceSlot(other.private_slot_.get()) :
      NULL);
  system_slot_.reset(
      other.system_slot_ ? PK11_ReferenceSlot(other.system_slot_.get()) : NULL);
}

NSSProfileFilterChromeOS::~NSSProfileFilterChromeOS() = default;

NSSProfileFilterChromeOS& NSSProfileFilterChromeOS::operator=(
    const NSSProfileFilterChromeOS& other) {
  public_slot_.reset(other.public_slot_ ?
      PK11_ReferenceSlot(other.public_slot_.get()) :
      NULL);
  private_slot_.reset(other.private_slot_ ?
      PK11_ReferenceSlot(other.private_slot_.get()) :
      NULL);
  system_slot_.reset(
      other.system_slot_ ? PK11_ReferenceSlot(other.system_slot_.get()) : NULL);
  return *this;
}

void NSSProfileFilterChromeOS::Init(crypto::ScopedPK11Slot public_slot,
                                    crypto::ScopedPK11Slot private_slot,
                                    crypto::ScopedPK11Slot system_slot) {
  // crypto::ScopedPK11Slot actually holds a reference counted object.
  // Because std::unique_ptr<T> assignment is a no-op if it already points to
  // the same pointer, a reference would be leaked because std::move() does
  // not release its reference, and the receiving object won't free
  // its copy.
  // TODO(dcheng): This comment doesn't seem quite right.
  if (public_slot_.get() != public_slot.get())
    public_slot_ = std::move(public_slot);
  if (private_slot_.get() != private_slot.get())
    private_slot_ = std::move(private_slot);
  if (system_slot_.get() != system_slot.get())
    system_slot_ = std::move(system_slot);
}

bool NSSProfileFilterChromeOS::IsModuleAllowed(PK11SlotInfo* slot) const {
  // If this is one of the public/private slots for this profile or the system
  // slot, allow it.
  if (slot == public_slot_.get() || slot == private_slot_.get() ||
      slot == system_slot_.get()) {
    return true;
  }
  // Allow the root certs module.
  if (PK11_HasRootCerts(slot))
    return true;
  // If it's from the read-only slots, allow it.
  if (PK11_IsInternal(slot) && !PK11_IsRemovable(slot))
    return true;
  // If |public_slot_| or |private_slot_| is null, there isn't a way to get the
  // modules to use in the final test.
  if (!public_slot_.get() || !private_slot_.get())
    return false;
  // If this is not the internal (file-system) module or the TPM module, allow
  // it. This would allow smartcards/etc, although ChromeOS doesn't currently
  // support that. (This assumes that private_slot_ and system_slot_ are on the
  // same module.)
  DCHECK(!system_slot_.get() ||
         PK11_GetModule(private_slot_.get()) ==
             PK11_GetModule(system_slot_.get()));
  SECMODModule* module_for_slot = PK11_GetModule(slot);
  if (module_for_slot != PK11_GetModule(public_slot_.get()) &&
      module_for_slot != PK11_GetModule(private_slot_.get())) {
    return true;
  }
  return false;
}

bool NSSProfileFilterChromeOS::IsCertAllowed(CERTCertificate* cert) const {
  crypto::ScopedPK11SlotList slots_for_cert(
      PK11_GetAllSlotsForCert(cert, NULL));
  if (!slots_for_cert) {
    DVLOG(2) << "cert no slots: " << base::StringPiece(cert->nickname);
    return false;
  }

  for (PK11SlotListElement* slot_element =
           PK11_GetFirstSafe(slots_for_cert.get());
       slot_element;
       slot_element =
           PK11_GetNextSafe(slots_for_cert.get(), slot_element, PR_FALSE)) {
    if (IsModuleAllowed(slot_element->slot)) {
      DVLOG(3) << "cert from " << CertSlotsString(cert)
               << " allowed: " << base::StringPiece(cert->nickname);
      PK11_FreeSlotListElement(slots_for_cert.get(), slot_element);
      return true;
    }
  }
  DVLOG(2) << "cert from " << CertSlotsString(cert)
           << " filtered: " << base::StringPiece(cert->nickname);
  return false;
}

}  // namespace net
