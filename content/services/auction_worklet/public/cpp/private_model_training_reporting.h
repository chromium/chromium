// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_PRIVATE_MODEL_TRAINING_REPORTING_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_PRIVATE_MODEL_TRAINING_REPORTING_H_

#include <cstdint>

namespace auction_worklet {

// Represents the maximum payload length that can be sent in a request for the
// Private Model Training API.
static constexpr uint32_t kMaxPayloadLength = 10000;

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_PRIVATE_MODEL_TRAINING_REPORTING_H_
