// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef GPU_COMMAND_BUFFER_COMMON_MAILBOX_H_
#define GPU_COMMAND_BUFFER_COMMON_MAILBOX_H_

#include <stdint.h>
#include <string.h>

#include <string>

#include "base/component_export.h"

// From gl2/gl2ext.h.
#ifndef GL_MAILBOX_SIZE_CHROMIUM
#define GL_MAILBOX_SIZE_CHROMIUM 16
#endif

namespace gpu {

// Importance to use in tracing. Higher values get the memory cost attributed,
// and equal values share the cost. We want the client to "win" over the
// service, since the service is acting on its behalf.
enum class TracingImportance : int {
  kNotOwner = 0,
  kServiceOwner = 1,
  kClientOwner = 2,
};

// A mailbox is an unguessable name that references a SharedImage.
// This name can be passed across processes permitting one process to share
// a SharedImage with another. The mailbox name consists of a random
// set of bytes, optionally with a checksum (in debug mode) to verify the
// name is valid.
struct COMPONENT_EXPORT(GPU_MAILBOX) Mailbox {
  using Name = int8_t[GL_MAILBOX_SIZE_CHROMIUM];

  Mailbox();

  static Mailbox FromVolatile(const volatile Mailbox& other) {
    // Because the copy constructor is trivial, const_cast is safe.
    return const_cast<const Mailbox&>(other);
  }

  bool IsZero() const;
  void SetZero();
  void SetName(const int8_t* name);

  // Generate a unique unguessable mailbox name.
  static Mailbox Generate();

  // Verify that the mailbox was created through Mailbox::Generate. This only
  // works in Debug (always returns true in Release). This is not a secure
  // check, only to catch bugs where clients forgot to call Mailbox::Generate.
  bool Verify() const;

  std::string ToDebugString() const;

  bool operator==(const Mailbox& other) const;
  std::strong_ordering operator<=>(const Mailbox& other) const;

  Name name;
};

}  // namespace gpu

template <>
struct std::hash<gpu::Mailbox> {
  std::size_t operator()(const gpu::Mailbox& m) const noexcept {
    // As the name is cryptographically random bytes, the first few bytes
    // should be more than sufficient as a hash.
    return static_cast<size_t>(m.name[0]) |
           (static_cast<size_t>(m.name[1]) << 8) |
           (static_cast<size_t>(m.name[2]) << 16) |
           (static_cast<size_t>(m.name[3]) << 24);
  }
};

#endif  // GPU_COMMAND_BUFFER_COMMON_MAILBOX_H_
