// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/mojom/authenticator_mojom_traits.h"  // nogncheck

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
  NOTREACHED_IN_MIGRATION();
  return blink::mojom::AuthenticatorTransport::USB;
}

// static
bool EnumTraits<blink::mojom::AuthenticatorTransport,
                device::FidoTransportProtocol>::
    FromMojom(blink::mojom::AuthenticatorTransport input,
              device::FidoTransportProtocol* output) {
  switch (input) {
    case blink::mojom::AuthenticatorTransport::USB:
      *output = ::device::FidoTransportProtocol::kUsbHumanInterfaceDevice;
      return true;
    case blink::mojom::AuthenticatorTransport::NFC:
      *output = ::device::FidoTransportProtocol::kNearFieldCommunication;
      return true;
    case blink::mojom::AuthenticatorTransport::BLE:
      *output = ::device::FidoTransportProtocol::kBluetoothLowEnergy;
      return true;
    case blink::mojom::AuthenticatorTransport::HYBRID:
      *output = ::device::FidoTransportProtocol::kHybrid;
      return true;
    case blink::mojom::AuthenticatorTransport::INTERNAL:
      *output = ::device::FidoTransportProtocol::kInternal;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
blink::mojom::PublicKeyCredentialType
EnumTraits<blink::mojom::PublicKeyCredentialType,
           device::CredentialType>::ToMojom(device::CredentialType input) {
  switch (input) {
    case ::device::CredentialType::kPublicKey:
      return blink::mojom::PublicKeyCredentialType::PUBLIC_KEY;
  }
  NOTREACHED_IN_MIGRATION();
  return blink::mojom::PublicKeyCredentialType::PUBLIC_KEY;
}

// static
bool EnumTraits<blink::mojom::PublicKeyCredentialType, device::CredentialType>::
    FromMojom(blink::mojom::PublicKeyCredentialType input,
              device::CredentialType* output) {
  switch (input) {
    case blink::mojom::PublicKeyCredentialType::PUBLIC_KEY:
      *output = ::device::CredentialType::kPublicKey;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
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
  NOTREACHED_IN_MIGRATION();
  return blink::mojom::AuthenticatorAttachment::NO_PREFERENCE;
}

// static
bool EnumTraits<blink::mojom::AuthenticatorAttachment,
                device::AuthenticatorAttachment>::
    FromMojom(blink::mojom::AuthenticatorAttachment input,
              device::AuthenticatorAttachment* output) {
  switch (input) {
    case blink::mojom::AuthenticatorAttachment::NO_PREFERENCE:
      *output = ::device::AuthenticatorAttachment::kAny;
      return true;
    case blink::mojom::AuthenticatorAttachment::PLATFORM:
      *output = ::device::AuthenticatorAttachment::kPlatform;
      return true;
    case blink::mojom::AuthenticatorAttachment::CROSS_PLATFORM:
      *output = ::device::AuthenticatorAttachment::kCrossPlatform;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
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
  NOTREACHED_IN_MIGRATION();
  return blink::mojom::ResidentKeyRequirement::DISCOURAGED;
}

// static
bool EnumTraits<blink::mojom::ResidentKeyRequirement,
                device::ResidentKeyRequirement>::
    FromMojom(blink::mojom::ResidentKeyRequirement input,
              device::ResidentKeyRequirement* output) {
  switch (input) {
    case blink::mojom::ResidentKeyRequirement::DISCOURAGED:
      *output = ::device::ResidentKeyRequirement::kDiscouraged;
      return true;
    case blink::mojom::ResidentKeyRequirement::PREFERRED:
      *output = ::device::ResidentKeyRequirement::kPreferred;
      return true;
    case blink::mojom::ResidentKeyRequirement::REQUIRED:
      *output = ::device::ResidentKeyRequirement::kRequired;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
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
  NOTREACHED_IN_MIGRATION();
  return blink::mojom::UserVerificationRequirement::REQUIRED;
}

// static
bool EnumTraits<blink::mojom::UserVerificationRequirement,
                device::UserVerificationRequirement>::
    FromMojom(blink::mojom::UserVerificationRequirement input,
              device::UserVerificationRequirement* output) {
  switch (input) {
    case blink::mojom::UserVerificationRequirement::REQUIRED:
      *output = ::device::UserVerificationRequirement::kRequired;
      return true;
    case blink::mojom::UserVerificationRequirement::PREFERRED:
      *output = ::device::UserVerificationRequirement::kPreferred;
      return true;
    case blink::mojom::UserVerificationRequirement::DISCOURAGED:
      *output = ::device::UserVerificationRequirement::kDiscouraged;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
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
  NOTREACHED_IN_MIGRATION();
  return blink::mojom::LargeBlobSupport::NOT_REQUESTED;
}

// static
bool EnumTraits<blink::mojom::LargeBlobSupport, device::LargeBlobSupport>::
    FromMojom(blink::mojom::LargeBlobSupport input,
              device::LargeBlobSupport* output) {
  switch (input) {
    case blink::mojom::LargeBlobSupport::NOT_REQUESTED:
      *output = ::device::LargeBlobSupport::kNotRequested;
      return true;
    case blink::mojom::LargeBlobSupport::REQUIRED:
      *output = ::device::LargeBlobSupport::kRequired;
      return true;
    case blink::mojom::LargeBlobSupport::PREFERRED:
      *output = ::device::LargeBlobSupport::kPreferred;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
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
bool StructTraits<blink::mojom::CableAuthenticationDataView,
                  device::CableDiscoveryData>::
    Read(blink::mojom::CableAuthenticationDataView data,
         device::CableDiscoveryData* out) {
  switch (data.version()) {
    case 1: {
      std::optional<std::array<uint8_t, 16>> client_eid, authenticator_eid;
      std::optional<std::array<uint8_t, 32>> session_pre_key;
      if (!data.ReadClientEid(&client_eid) || !client_eid ||
          !data.ReadAuthenticatorEid(&authenticator_eid) ||
          !authenticator_eid || !data.ReadSessionPreKey(&session_pre_key) ||
          !session_pre_key) {
        return false;
      }

      out->version = device::CableDiscoveryData::Version::V1;
      out->v1.emplace();
      out->v1->client_eid = *client_eid;
      out->v1->authenticator_eid = *authenticator_eid;
      out->v1->session_pre_key = *session_pre_key;
      break;
    }

    case 2: {
      std::optional<std::vector<uint8_t>> server_link_data;
      std::optional<std::vector<uint8_t>> experiments;
      if (!data.ReadServerLinkData(&server_link_data) || !server_link_data ||
          !data.ReadExperiments(&experiments) || !experiments) {
        return false;
      }

      out->version = device::CableDiscoveryData::Version::V2;
      out->v2.emplace(std::move(*server_link_data), std::move(*experiments));

      break;
    }

    default:
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
  NOTREACHED_IN_MIGRATION();
  return blink::mojom::AttestationConveyancePreference::NONE;
}

// static
bool EnumTraits<blink::mojom::AttestationConveyancePreference,
                device::AttestationConveyancePreference>::
    FromMojom(blink::mojom::AttestationConveyancePreference input,
              device::AttestationConveyancePreference* output) {
  switch (input) {
    case blink::mojom::AttestationConveyancePreference::NONE:
      *output = ::device::AttestationConveyancePreference::kNone;
      return true;
    case blink::mojom::AttestationConveyancePreference::INDIRECT:
      *output = ::device::AttestationConveyancePreference::kIndirect;
      return true;
    case blink::mojom::AttestationConveyancePreference::DIRECT:
      *output = ::device::AttestationConveyancePreference::kDirect;
      return true;
    case blink::mojom::AttestationConveyancePreference::ENTERPRISE:
      *output = ::device::AttestationConveyancePreference::
          kEnterpriseIfRPListedOnAuthenticator;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace mojo
