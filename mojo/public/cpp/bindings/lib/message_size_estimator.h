// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_SIZE_ESTIMATOR_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_SIZE_ESTIMATOR_H_

#include <stddef.h>
#include <stdint.h>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/moving_window.h"

namespace mojo::internal {

// TODO(andreaorru): Add a template parameter to help construct
// a vector of the right size for the interface.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) MessageSizeEstimator {
 public:
  MessageSizeEstimator();
  ~MessageSizeEstimator();

  // Enables predictive allocation for a given message type in an interface.
  // |message_name| is the shared-message-id of the message.
  void EnablePredictiveAllocation(uint32_t message_name);

  // Estimates the payload size based on the history of previous allocations
  // for |message_name|. If predictive allocation was not enabled for the given
  // message, returns 0 (the default value for |mojo::Message|).
  size_t EstimatePayloadSize(uint32_t message_name) const;

  // Keeps track of the payload size of |message_name|, to inform subsequent
  // allocations. If predictive allocation was not enabled for the given
  // message, this method does nothing.
  void TrackPayloadSize(uint32_t message_name, size_t size);

 private:
  using SlidingWindow = base::MovingMax<size_t>;

  base::flat_map<uint32_t, std::unique_ptr<SlidingWindow>> samples_;
};

}  // namespace mojo::internal

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_SIZE_ESTIMATOR_H_
