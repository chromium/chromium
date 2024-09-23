// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_EMBEDDER_CONFIGURATION_H_
#define MOJO_CORE_EMBEDDER_CONFIGURATION_H_

#include <stddef.h>
#include <stdint.h>

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

  // Forcibly disables the Mojo's ipcz-based implementation. This is an
  // alternative to manual feature override for applications which don't use
  // base::FeatureList.
  //
  // TODO(crbug.com/40058840): Remove this once dependents are gone.
  bool disable_ipcz = false;

  // If |true|, this process will always attempt to allocate shared memory
  // directly rather than synchronously delegating to a broker process where
  // applicable.
  //
  // This is useful to set in processes which are not acting as the broker but
  // which are otherwise sufficiently privileged to allocate named shared memory
  // objects.
  bool force_direct_shared_memory_allocation = false;

  // Maximum number of active memory mappings.
  size_t max_mapping_table_size = 1000000;

  // Maximum data size of messages sent over message pipes, in bytes.
  size_t max_message_num_bytes = 256 * 1024 * 1024;

  // Maximum size of a single shared memory segment, in bytes.
  size_t max_shared_memory_num_bytes = 1024 * 1024 * 1024;

  // If true we will not advertise our capabilities to our peer.
  bool dont_advertise_capabilities = false;
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_EMBEDDER_CONFIGURATION_H_
