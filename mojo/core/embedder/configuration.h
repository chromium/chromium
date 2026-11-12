// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_EMBEDDER_CONFIGURATION_H_
#define MOJO_CORE_EMBEDDER_CONFIGURATION_H_

namespace mojo {
namespace core {

// A set of configuration parameters that the Mojo system uses internally. The
// configuration used can be overridden from the default by passing a
// Configuration into |mojo::core::Init()|. See embedder.h.
//
// NOTE: Please ensure that this type remains a simple aggregate of POD fields.
struct Configuration {
  // Indicates whether this process should act as the sole broker process within
  // its graph of interconnected Mojo-embedder processes. This setting is only
  // relevant in multiprocess environments.
  bool is_broker_process = false;

  // If |true|, this process will always attempt to allocate shared memory
  // directly rather than synchronously delegating to a broker process where
  // applicable.
  //
  // This is useful to set in processes which are not acting as the broker but
  // which are otherwise sufficiently privileged to allocate named shared memory
  // objects.
  bool force_direct_shared_memory_allocation = false;
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_EMBEDDER_CONFIGURATION_H_
