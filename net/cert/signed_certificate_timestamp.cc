// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/signed_certificate_timestamp.h"

#include "base/pickle.h"

namespace net {

namespace ct {

bool SignedCertificateTimestamp::LessThan::operator()(
    const scoped_refptr<SignedCertificateTimestamp>& lhs,
    const scoped_refptr<SignedCertificateTimestamp>& rhs) const {
  if (lhs.get() == rhs.get())
    return false;
  if (lhs->signature.signature_data != rhs->signature.signature_data)
    return lhs->signature.signature_data < rhs->signature.signature_data;
  if (lhs->log_id != rhs->log_id)
    return lhs->log_id < rhs->log_id;
  if (lhs->timestamp != rhs->timestamp)
    return lhs->timestamp < rhs->timestamp;
  if (lhs->extensions != rhs->extensions)
    return lhs->extensions < rhs->extensions;
  if (lhs->origin != rhs->origin)
    return lhs->origin < rhs->origin;
  return lhs->version < rhs->version;
}

SignedCertificateTimestamp::SignedCertificateTimestamp()
    : version(V1), origin(SCT_EMBEDDED) {}

SignedCertificateTimestamp::~SignedCertificateTimestamp() = default;

void SignedCertificateTimestamp::Persist(base::Pickle* pickle) {
  pickle->WriteInt(version);
  pickle->WriteString(log_id);
  pickle->WriteInt64(timestamp.ToInternalValue());
  pickle->WriteString(extensions);
  pickle->WriteInt(signature.hash_algorithm);
  pickle->WriteInt(signature.signature_algorithm);
  pickle->WriteString(signature.signature_data);
  pickle->WriteInt(origin);
  pickle->WriteString(log_description);
}

// static
scoped_refptr<SignedCertificateTimestamp>
SignedCertificateTimestamp::CreateFromPickle(base::PickleIterator* iter) {
  int version;
  int64_t timestamp;
  int hash_algorithm;
  int sig_algorithm;
  scoped_refptr<SignedCertificateTimestamp> sct(
      new SignedCertificateTimestamp());
  int origin;
  // string values are set directly
  if (!(iter->ReadInt(&version) &&
        iter->ReadString(&sct->log_id) &&
        iter->ReadInt64(&timestamp) &&
        iter->ReadString(&sct->extensions) &&
        iter->ReadInt(&hash_algorithm) &&
        iter->ReadInt(&sig_algorithm) &&
        iter->ReadString(&sct->signature.signature_data) &&
        iter->ReadInt(&origin) &&
        iter->ReadString(&sct->log_description))) {
    return nullptr;
  }
  // Now set the rest of the member variables:
  sct->version = static_cast<Version>(version);
  sct->timestamp = base::Time::FromInternalValue(timestamp);
  sct->signature.hash_algorithm =
      static_cast<DigitallySigned::HashAlgorithm>(hash_algorithm);
  sct->signature.signature_algorithm =
      static_cast<DigitallySigned::SignatureAlgorithm>(sig_algorithm);
  sct->origin = static_cast<Origin>(origin);
  return sct;
}

SignedEntryData::SignedEntryData() : type(LOG_ENTRY_TYPE_X509) {}

SignedEntryData::~SignedEntryData() = default;

void SignedEntryData::Reset() {
  type = SignedEntryData::LOG_ENTRY_TYPE_X509;
  leaf_certificate.clear();
  tbs_certificate.clear();
}

DigitallySigned::DigitallySigned()
    : hash_algorithm(HASH_ALGO_NONE), signature_algorithm(SIG_ALGO_ANONYMOUS) {}

DigitallySigned::~DigitallySigned() = default;

bool DigitallySigned::SignatureParametersMatch(
    HashAlgorithm other_hash_algorithm,
    SignatureAlgorithm other_signature_algorithm) const {
  return (hash_algorithm == other_hash_algorithm) &&
         (signature_algorithm == other_signature_algorithm);
}
}  // namespace ct

}  // namespace net
