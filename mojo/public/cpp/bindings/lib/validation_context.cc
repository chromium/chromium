// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/validation_context.h"

#include "base/logging.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {
namespace internal {

ValidationContext::ValidationContext(const void* data,
                                     size_t data_num_bytes,
                                     size_t num_handles,
                                     size_t num_associated_endpoint_handles,
                                     Message* message,
                                     const char* description,
                                     int stack_depth,
                                     ValidatorType validator_type)
    : message_(message),
      description_(description),
      validator_type_(validator_type),
      data_begin_(reinterpret_cast<uintptr_t>(data)),
      data_end_(data_begin_ + data_num_bytes),
      handle_begin_(0),
      handle_end_(static_cast<uint32_t>(num_handles)),
      associated_endpoint_handle_begin_(0),
      associated_endpoint_handle_end_(
          static_cast<uint32_t>(num_associated_endpoint_handles)),
      stack_depth_(stack_depth) {
  // Check whether the calculation of |data_end_| or static_cast from size_t to
  // uint32_t causes overflow.
  // They shouldn't happen but they do, set the corresponding range to empty.
  if (data_end_ < data_begin_) {
    NOTREACHED();
    data_end_ = data_begin_;
  }
  if (handle_end_ < num_handles) {
    NOTREACHED();
    handle_end_ = 0;
  }
  if (associated_endpoint_handle_end_ < num_associated_endpoint_handles) {
    NOTREACHED();
    associated_endpoint_handle_end_ = 0;
  }
}

ValidationContext::ValidationContext(Message* message,
                                     const char* description,
                                     ValidatorType validator_type)
    : ValidationContext(message->payload(),
                        message->payload_num_bytes(),
                        message->handles()->size(),
                        message->payload_num_interface_ids(),
                        message,
                        description,
                        0,
                        validator_type) {}

ValidationContext::~ValidationContext() {
}

std::string ValidationContext::GetFullDescription() const {
  std::string full_description(description_);
  switch (validator_type_) {
    case kUnspecifiedValidator:
    case kRequestValidator:
      break;
    case kResponseValidator:
      full_description += " response";
      break;
  }
  return full_description;
}

}  // namespace internal
}  // namespace mojo
