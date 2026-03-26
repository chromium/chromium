// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/mojom/authenticator_mojom_traits.h"  // nogncheck

#include "base/notreached.h"

namespace mojo {

// static
blink::mojom::AuthenticatorTransport
EnumTraits<blink::mojom::AuthenticatorTransport,
           device::FidoTransportProtocol>::ToMojom(device::FidoTransportProtocol
                                                       input) {
  switch (input) {
    case ::device::FidoTransportProtocol::kUsbHumanInterfaceDevice:
      return blink::mojom::AuthenticatorTransport::USB;
    case ::device::FidoTransportProtocol::kNearFieldCommunication:
      return blink::mojom::AuthenticatorTransport::NFC;
    case ::device::FidoTransportProtocol::kBluetoothLowEnergy:
      return blink::mojom::AuthenticatorTransport::BLE;
    case ::device::FidoTransportProtocol::kHybrid:
      return blink::mojom::AuthenticatorTransport::HYBRID;
    case ::device::FidoTransportProtocol::kInternal:
      return blink::mojom::AuthenticatorTransport::INTERNAL;
    case ::device::FidoTransportProtocol::kDeprecatedAoa:
      return blink::mojom::AuthenticatorTransport::HYBRID;
  }
  NOTREACHED();
}

// static
device::FidoTransportProtocol EnumTraits<blink::mojom::AuthenticatorTransport,
                                         device::FidoTransportProtocol>::
    FromMojom(blink::mojom::AuthenticatorTransport input) {
  switch (input) {
    case blink::mojom::AuthenticatorTransport::USB:
      return ::device::FidoTransportProtocol::kUsbHumanInterfaceDevice;
    case blink::mojom::AuthenticatorTransport::NFC:
      return ::device::FidoTransportProtocol::kNearFieldCommunication;
    case blink::mojom::AuthenticatorTransport::BLE:
      return ::device::FidoTransportProtocol::kBluetoothLowEnergy;
    case blink::mojom::AuthenticatorTransport::HYBRID:
      return ::device::FidoTransportProtocol::kHybrid;
    case blink::mojom::AuthenticatorTransport::INTERNAL:
      return ::device::FidoTransportProtocol::kInternal;
  }
  NOTREACHED();
}

// static
blink::mojom::PublicKeyCredentialType
EnumTraits<blink::mojom::PublicKeyCredentialType,
           device::CredentialType>::ToMojom(device::CredentialType input) {
  switch (input) {
    case ::device::CredentialType::kPublicKey:
      return blink::mojom::PublicKeyCredentialType::PUBLIC_KEY;
  }
  NOTREACHED();
}

// static
device::CredentialType
EnumTraits<blink::mojom::PublicKeyCredentialType, device::CredentialType>::
    FromMojom(blink::mojom::PublicKeyCredentialType input) {
  switch (input) {
    case blink::mojom::PublicKeyCredentialType::PUBLIC_KEY:
      return ::device::CredentialType::kPublicKey;
  }
  NOTREACHED();
}

// static
bool StructTraits<blink::mojom::PublicKeyCredentialParametersDataView,
                  device::PublicKeyCredentialParams::CredentialInfo>::
    Read(blink::mojom::PublicKeyCredentialParametersDataView data,
         device::PublicKeyCredentialParams::CredentialInfo* out) {
  out->algorithm = data.algorithm_identifier();
  if (data.ReadType(&out->type)) {
    return true;
  }
  return false;
}

// static
bool StructTraits<blink::mojom::PublicKeyCredentialDescriptorDataView,
                  device::PublicKeyCredentialDescriptor>::
    Read(blink::mojom::PublicKeyCredentialDescriptorDataView data,
         device::PublicKeyCredentialDescriptor* out) {
  device::CredentialType type;
  std::vector<uint8_t> id;
  std::vector<device::FidoTransportProtocol> protocols;
  if (!data.ReadType(&type) || !data.ReadId(&id) ||
      !data.ReadTransports(&protocols)) {
    return false;
  }
  device::PublicKeyCredentialDescriptor descriptor(type, id,
                                                   {std::move(protocols)});
  *out = descriptor;
  return true;
}

// static
blink::mojom::AuthenticatorAttachment EnumTraits<
    blink::mojom::AuthenticatorAttachment,
    device::AuthenticatorAttachment>::ToMojom(device::AuthenticatorAttachment
                                                  input) {
  switch (input) {
    case ::device::AuthenticatorAttachment::kAny:
      return blink::mojom::AuthenticatorAttachment::NO_PREFERENCE;
    case ::device::AuthenticatorAttachment::kPlatform:
      return blink::mojom::AuthenticatorAttachment::PLATFORM;
    case ::device::AuthenticatorAttachment::kCrossPlatform:
      return blink::mojom::AuthenticatorAttachment::CROSS_PLATFORM;
  }
  NOTREACHED();
}

// static
device::AuthenticatorAttachment
EnumTraits<blink::mojom::AuthenticatorAttachment,
           device::AuthenticatorAttachment>::
    FromMojom(blink::mojom::AuthenticatorAttachment input) {
  switch (input) {
    case blink::mojom::AuthenticatorAttachment::NO_PREFERENCE:
      return ::device::AuthenticatorAttachment::kAny;
    case blink::mojom::AuthenticatorAttachment::PLATFORM:
      return ::device::AuthenticatorAttachment::kPlatform;
    case blink::mojom::AuthenticatorAttachment::CROSS_PLATFORM:
      return ::device::AuthenticatorAttachment::kCrossPlatform;
  }
  NOTREACHED();
}

// static
blink::mojom::ResidentKeyRequirement EnumTraits<
    blink::mojom::ResidentKeyRequirement,
    device::ResidentKeyRequirement>::ToMojom(device::ResidentKeyRequirement
                                                 input) {
  switch (input) {
    case ::device::ResidentKeyRequirement::kDiscouraged:
      return blink::mojom::ResidentKeyRequirement::DISCOURAGED;
    case ::device::ResidentKeyRequirement::kPreferred:
      return blink::mojom::ResidentKeyRequirement::PREFERRED;
    case ::device::ResidentKeyRequirement::kRequired:
      return blink::mojom::ResidentKeyRequirement::REQUIRED;
  }
  NOTREACHED();
}

// static
device::ResidentKeyRequirement EnumTraits<blink::mojom::ResidentKeyRequirement,
                                          device::ResidentKeyRequirement>::
    FromMojom(blink::mojom::ResidentKeyRequirement input) {
  switch (input) {
    case blink::mojom::ResidentKeyRequirement::DISCOURAGED:
      return ::device::ResidentKeyRequirement::kDiscouraged;
    case blink::mojom::ResidentKeyRequirement::PREFERRED:
      return ::device::ResidentKeyRequirement::kPreferred;
    case blink::mojom::ResidentKeyRequirement::REQUIRED:
      return ::device::ResidentKeyRequirement::kRequired;
  }
  NOTREACHED();
}

// static
blink::mojom::UserVerificationRequirement
EnumTraits<blink::mojom::UserVerificationRequirement,
           device::UserVerificationRequirement>::
    ToMojom(device::UserVerificationRequirement input) {
  switch (input) {
    case ::device::UserVerificationRequirement::kRequired:
      return blink::mojom::UserVerificationRequirement::REQUIRED;
    case ::device::UserVerificationRequirement::kPreferred:
      return blink::mojom::UserVerificationRequirement::PREFERRED;
    case ::device::UserVerificationRequirement::kDiscouraged:
      return blink::mojom::UserVerificationRequirement::DISCOURAGED;
  }
  NOTREACHED();
}

// static
device::UserVerificationRequirement
EnumTraits<blink::mojom::UserVerificationRequirement,
           device::UserVerificationRequirement>::
    FromMojom(blink::mojom::UserVerificationRequirement input) {
  switch (input) {
    case blink::mojom::UserVerificationRequirement::REQUIRED:
      return ::device::UserVerificationRequirement::kRequired;
    case blink::mojom::UserVerificationRequirement::PREFERRED:
      return ::device::UserVerificationRequirement::kPreferred;
    case blink::mojom::UserVerificationRequirement::DISCOURAGED:
      return ::device::UserVerificationRequirement::kDiscouraged;
  }
  NOTREACHED();
}

// static
blink::mojom::LargeBlobSupport
EnumTraits<blink::mojom::LargeBlobSupport, device::LargeBlobSupport>::ToMojom(
    device::LargeBlobSupport input) {
  switch (input) {
    case ::device::LargeBlobSupport::kNotRequested:
      return blink::mojom::LargeBlobSupport::NOT_REQUESTED;
    case ::device::LargeBlobSupport::kRequired:
      return blink::mojom::LargeBlobSupport::REQUIRED;
    case ::device::LargeBlobSupport::kPreferred:
      return blink::mojom::LargeBlobSupport::PREFERRED;
  }
  NOTREACHED();
}

// static
device::LargeBlobSupport
EnumTraits<blink::mojom::LargeBlobSupport, device::LargeBlobSupport>::FromMojom(
    blink::mojom::LargeBlobSupport input) {
  switch (input) {
    case blink::mojom::LargeBlobSupport::NOT_REQUESTED:
      return ::device::LargeBlobSupport::kNotRequested;
    case blink::mojom::LargeBlobSupport::REQUIRED:
      return ::device::LargeBlobSupport::kRequired;
    case blink::mojom::LargeBlobSupport::PREFERRED:
      return ::device::LargeBlobSupport::kPreferred;
  }
  NOTREACHED();
}

// static
bool StructTraits<blink::mojom::AuthenticatorSelectionCriteriaDataView,
                  device::AuthenticatorSelectionCriteria>::
    Read(blink::mojom::AuthenticatorSelectionCriteriaDataView data,
         device::AuthenticatorSelectionCriteria* out) {
  device::AuthenticatorAttachment authenticator_attachment;
  device::UserVerificationRequirement user_verification_requirement;
  device::ResidentKeyRequirement resident_key;
  if (!data.ReadAuthenticatorAttachment(&authenticator_attachment) ||
      !data.ReadUserVerification(&user_verification_requirement) ||
      !data.ReadResidentKey(&resident_key)) {
    return false;
  }

  *out = device::AuthenticatorSelectionCriteria(
      authenticator_attachment, resident_key, user_verification_requirement);
  return true;
}

// static
bool StructTraits<blink::mojom::PublicKeyCredentialRpEntityDataView,
                  device::PublicKeyCredentialRpEntity>::
    Read(blink::mojom::PublicKeyCredentialRpEntityDataView data,
         device::PublicKeyCredentialRpEntity* out) {
  if (!data.ReadId(&out->id) || !data.ReadName(&out->name)) {
    return false;
  }

  return true;
}

// static
bool StructTraits<blink::mojom::PublicKeyCredentialUserEntityDataView,
                  device::PublicKeyCredentialUserEntity>::
    Read(blink::mojom::PublicKeyCredentialUserEntityDataView data,
         device::PublicKeyCredentialUserEntity* out) {
  if (!data.ReadId(&out->id) || !data.ReadName(&out->name) ||
      !data.ReadDisplayName(&out->display_name)) {
    return false;
  }

  return true;
}

// static
blink::mojom::AttestationConveyancePreference
EnumTraits<blink::mojom::AttestationConveyancePreference,
           device::AttestationConveyancePreference>::
    ToMojom(device::AttestationConveyancePreference input) {
  switch (input) {
    case ::device::AttestationConveyancePreference::kNone:
      return blink::mojom::AttestationConveyancePreference::NONE;
    case ::device::AttestationConveyancePreference::kIndirect:
      return blink::mojom::AttestationConveyancePreference::INDIRECT;
    case ::device::AttestationConveyancePreference::kDirect:
      return blink::mojom::AttestationConveyancePreference::DIRECT;
    case ::device::AttestationConveyancePreference::
        kEnterpriseIfRPListedOnAuthenticator:
      return blink::mojom::AttestationConveyancePreference::ENTERPRISE;
    case ::device::AttestationConveyancePreference::
        kEnterpriseApprovedByBrowser:
      return blink::mojom::AttestationConveyancePreference::ENTERPRISE;
  }
  NOTREACHED();
}

// static
device::AttestationConveyancePreference
EnumTraits<blink::mojom::AttestationConveyancePreference,
           device::AttestationConveyancePreference>::
    FromMojom(blink::mojom::AttestationConveyancePreference input) {
  switch (input) {
    case blink::mojom::AttestationConveyancePreference::NONE:
      return ::device::AttestationConveyancePreference::kNone;
    case blink::mojom::AttestationConveyancePreference::INDIRECT:
      return ::device::AttestationConveyancePreference::kIndirect;
    case blink::mojom::AttestationConveyancePreference::DIRECT:
      return ::device::AttestationConveyancePreference::kDirect;
    case blink::mojom::AttestationConveyancePreference::ENTERPRISE:
      return ::device::AttestationConveyancePreference::
          kEnterpriseIfRPListedOnAuthenticator;
  }
  NOTREACHED();
}

}  // namespace mojo
