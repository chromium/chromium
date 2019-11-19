// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/message_header_validator.h"

#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/validate_params.h"
#include "mojo/public/cpp/bindings/lib/validation_context.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/cpp/bindings/lib/validation_util.h"

namespace mojo {
namespace {

// TODO(yzshen): Define a mojom struct for message header and use the generated
// validation and data view code.
bool IsValidMessageHeader(const internal::MessageHeader* header,
                          internal::ValidationContext* validation_context) {
  // NOTE: Our goal is to preserve support for future extension of the message
  // header. If we encounter fields we do not understand, we must ignore them.

  // Extra validation of the struct header:
  do {
    if (header->version == 0) {
      if (header->num_bytes == sizeof(internal::MessageHeader))
        break;
    } else if (header->version == 1) {
      if (header->num_bytes == sizeof(internal::MessageHeaderV1))
        break;
    } else if (header->version == 2) {
      if (header->num_bytes == sizeof(internal::MessageHeaderV2))
        break;
    } else if (header->version > 2) {
      if (header->num_bytes >= sizeof(internal::MessageHeaderV2))
        break;
    }
    internal::ReportValidationError(
        validation_context,
        internal::VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER);
    return false;
  } while (false);

  // Validate flags (allow unknown bits):

  // These flags require a RequestID.
  constexpr uint32_t kRequestIdFlags =
      Message::kFlagExpectsResponse | Message::kFlagIsResponse;
  if (header->version == 0 && (header->flags & kRequestIdFlags)) {
    internal::ReportValidationError(
        validation_context,
        internal::VALIDATION_ERROR_MESSAGE_HEADER_MISSING_REQUEST_ID);
    return false;
  }

  // These flags are mutually exclusive.
  if ((header->flags & kRequestIdFlags) == kRequestIdFlags) {
    internal::ReportValidationError(
        validation_context,
        internal::VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAGS);
    return false;
  }

  if (header->version < 2)
    return true;

  auto* header_v2 = static_cast<const internal::MessageHeaderV2*>(header);
  // For the payload pointer:
  // - Check that the pointer can be safely decoded.
  // - Claim one byte that the pointer points to. It makes sure not only the
  //   address is within the message, but also the address precedes the array
  //   storing interface IDs (which is important for safely calculating the
  //   payload size).
  // - Validation of the payload contents will be done separately based on the
  //   payload type.
  if (!internal::ValidatePointerNonNullable(header_v2->payload, 5,
                                            validation_context) ||
      !internal::ValidatePointer(header_v2->payload, validation_context) ||
      !validation_context->ClaimMemory(header_v2->payload.Get(), 1)) {
    return false;
  }

  const internal::ContainerValidateParams validate_params(0, false, nullptr);
  if (!internal::ValidateContainer(header_v2->payload_interface_ids,
                                   validation_context, &validate_params)) {
    return false;
  }

  if (!header_v2->payload_interface_ids.is_null()) {
    size_t num_ids = header_v2->payload_interface_ids.Get()->size();
    const uint32_t* ids = header_v2->payload_interface_ids.Get()->storage();
    for (size_t i = 0; i < num_ids; ++i) {
      if (!IsValidInterfaceId(ids[i]) || IsMasterInterfaceId(ids[i])) {
        internal::ReportValidationError(
            validation_context,
            internal::VALIDATION_ERROR_ILLEGAL_INTERFACE_ID);
        return false;
      }
    }
  }

  return true;
}

}  // namespace

MessageHeaderValidator::MessageHeaderValidator()
    : MessageHeaderValidator("MessageHeaderValidator") {}

MessageHeaderValidator::MessageHeaderValidator(const std::string& description)
    : description_(description) {
}

void MessageHeaderValidator::SetDescription(const std::string& description) {
  description_ = description;
}

bool MessageHeaderValidator::Accept(Message* message) {
  // Don't bother validating unserialized message headers.
  if (!message->is_serialized())
    return true;

  // Pass 0 as number of handles and associated endpoint handles because we
  // don't expect any in the header, even if |message| contains handles.
  internal::ValidationContext validation_context(
      message->data(), message->data_num_bytes(), 0, 0, message,
      description_.c_str());

  if (!internal::ValidateStructHeaderAndClaimMemory(message->data(),
                                                    &validation_context))
    return false;

  if (!IsValidMessageHeader(message->header(), &validation_context))
    return false;

  return true;
}

}  // namespace mojo
