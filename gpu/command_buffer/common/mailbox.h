// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

namespace content {
class PPB_Graphics3D_Impl;
}

namespace media {
class GLES2DecoderHelperImpl;
}

namespace gpu {

namespace gles2 {
class GLES2Implementation;
}

// A mailbox is an unguessable name that references texture image data.
// This name can be passed across processes permitting one context to share
// texture image data with another. The mailbox name consists of a random
// set of bytes, optionally with a checksum (in debug mode) to verify the
// name is valid.
// See src/gpu/GLES2/extensions/CHROMIUM/CHROMIUM_texture_mailbox.txt for more
// details.
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

  // Indicates whether this mailbox is used with the SharedImage system.
  bool IsSharedImage() const;

  // Generate a unique unguessable mailbox name for use with the SharedImage
  // system.
  static Mailbox GenerateForSharedImage();

  // Verify that the mailbox was created through Mailbox::Generate. This only
  // works in Debug (always returns true in Release). This is not a secure
  // check, only to catch bugs where clients forgot to call Mailbox::Generate.
  bool Verify() const;

  std::string ToDebugString() const;

  Name name;

  bool operator<(const Mailbox& other) const {
    return memcmp(this, &other, sizeof other) < 0;
  }
  bool operator==(const Mailbox& other) const {
    return memcmp(this, &other, sizeof other) == 0;
  }
  bool operator!=(const Mailbox& other) const {
    return !operator==(other);
  }

 private:
  // Generate a unique unguessable mailbox name for use with the legacy mailbox
  // system.
  // NOTE: We are in the process of eliminating this method. DO NOT ADD ANY NEW
  // USAGES - instead, reach out to shared-image-team@ with your use case. See
  // crbug.com/1273084.
  static Mailbox GenerateLegacyMailbox();

  friend class content::PPB_Graphics3D_Impl;
  friend class gles2::GLES2Implementation;
  friend class media::GLES2DecoderHelperImpl;

 public:
  // Generate a legacy mailbox for usage in tests of production code that
  // still interacts with the legacy mailbox system.
  static Mailbox GenerateLegacyMailboxForTesting() {
    return GenerateLegacyMailbox();
  }
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_MAILBOX_H_
