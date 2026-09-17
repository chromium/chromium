// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_HEADER_VALIDATOR_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_HEADER_VALIDATOR_H_

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/containers/span.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {

namespace internal {

// Validates the header of the serialized message in `data`.
//
// `data` must contain the entire message (header and payload).
//
// Returns false if the data fails validation. If `message` is
// non-null, it is also reported as bad; otherwise, reporting
// is the caller's responsibility.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool ValidateMessageHeader(base::span<const uint8_t> data,
                           Message* message = nullptr,
                           const char* description = "");

}  // namespace internal

class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) MessageHeaderValidator
    : public MessageReceiver {
 public:
  MessageHeaderValidator();
  explicit MessageHeaderValidator(const std::string& description);

  // Sets the description associated with this validator. Used for reporting
  // more detailed validation errors.
  void SetDescription(const std::string& description);

  bool Accept(Message* message) override;

 private:
  std::string description_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_HEADER_VALIDATOR_H_
