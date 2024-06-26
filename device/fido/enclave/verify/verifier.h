// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_VERIFY_VERIFIER_H_
#define DEVICE_FIDO_ENCLAVE_VERIFY_VERIFIER_H_

#include <optional>

#include "base/time/time.h"
#include "device/fido/enclave/verify/proto/expected_value.pb.h"

namespace device::enclave {

class KernelAttachment;
class FirmwareAttachment;
class RootLayerData;
class Endorsements;
class OakContainersExpectedValues;
class OakContainersReferenceValues;
class OakContainersEndorsements;
class OakRestrictedKernelData;
class OakRestrictedKernelReferenceValues;
class OakRestrictedKernelEndorsements;
class TransparentReleaseEndorsement;
class EndorsementReferenceValue;
class KernelExpectedValues;
class KernelBinaryReferenceValue;
class KernelLayerEndorsements;
class KernelLayerReferenceValues;
class RootLayerExpectedValues;
class RootLayerEndorsements;
class RootLayerReferenceValues;
class BinaryReferenceValue;
class ReferenceValues;
class ApplicationLayerReferenceValues;
class ApplicationLayerEndorsements;

// Extract the KernelAttachment data from the provided Endorsement
// It will only be returned if the endorsement was verified.
std::optional<KernelAttachment> GetVerifiedKernelAttachment(
    base::Time now,
    const TransparentReleaseEndorsement& endorsement,
    const EndorsementReferenceValue& public_keys);

// Get the expected values from the provided TransportReleaseEndorsement.
// The endorsement is expected to contain a subject that can be deserialized as
// a KernelAttachment.
// The subject itself will be verified, and then the image and setup_data
// expected values will be returned.
// Subsequent callers can provide just the cached image and setup_data digests.
std::optional<KernelExpectedValues> GetKernelExpectedValues(
    base::Time now,
    std::optional<const TransparentReleaseEndorsement&> endorsement,
    const KernelBinaryReferenceValue& reference_value);

bool VerifyRootLayerMeasurementDigests(
    const RootLayerData& values,
    const RootLayerExpectedValues& expected_values);

// Validates the values extracted from the evidence against the reference
// values and endorsements for Oak Restricted Kernel applications.
bool VerifyOakRestrictedKernelMeasurementDigests(
    const OakRestrictedKernelData& values,
    const OakRestrictedKernelExpectedValues& expected);

ExpectedDigests IntoExpectedDigests(const RawDigest& source);

// Extract the stage0 data from the provided Endorsement
// It will only be returned if the endorsement was verified.
std::optional<FirmwareAttachment> GetVerifiedStage0Attachment(
    base::Time now,
    const TransparentReleaseEndorsement& endorsement,
    const EndorsementReferenceValue& public_keys);

// Get the expected values from the provided TransparentReleaseEndorsement.
// The endorsement is expected to contain a subject that can be deserialized as
// a FirmwareAttachment.
// The subject itself will be verified, and then the expected digests will be
// verified. Each digest corresponds to a number of vCPU, and any of them are a
// valid match for the digest in the evidence.
std::optional<ExpectedDigests> GetStage0ExpectedValues(
    base::Time now,
    std::optional<const TransparentReleaseEndorsement&> endorsement,
    const BinaryReferenceValue& reference_value);

std::optional<ApplicationLayerExpectedValues> GetApplicationLayerExpectedValues(
    base::Time now,
    std::optional<const ApplicationLayerEndorsements&> endorsements,
    const ApplicationLayerReferenceValues& reference_values);

std::optional<KernelLayerExpectedValues> GetKernelLayerExpectedValues(
    base::Time now,
    std::optional<const KernelLayerEndorsements&> endorsements,
    const KernelLayerReferenceValues& reference_values);

std::optional<RootLayerExpectedValues> GetRootLayerExpectedValues(
    base::Time now,
    std::optional<const RootLayerEndorsements&> endorsements,
    const RootLayerReferenceValues& reference_values);

std::optional<OakContainersExpectedValues> GetOakContainersExpectedValues(
    base::Time now,
    const OakContainersEndorsements& endorsements,
    const OakContainersReferenceValues& reference_values);

std::optional<OakRestrictedKernelExpectedValues>
GetOakRestrictedKernelExpectedValues(
    base::Time now,
    const OakRestrictedKernelEndorsements& endorsements,
    const OakRestrictedKernelReferenceValues& reference_values);

std::optional<ExpectedValues> GetExpectedValues(
    base::Time now,
    const Endorsements& endorsements,
    const ReferenceValues& reference_values);

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_VERIFY_VERIFIER_H_
