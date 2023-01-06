// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/mailbox.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/check.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"

namespace gpu {
namespace {

// The last byte of the mailbox's name stores the SharedImage flag. This avoids
// conflicts with Verify logic, which uses the first byte.
constexpr size_t kSharedImageFlagIndex = GL_MAILBOX_SIZE_CHROMIUM - 1;

// Use the lowest bit for the SharedImage flag (any bit would work).
constexpr int8_t kSharedImageFlag = 0x1;

void MarkMailboxAsSharedImage(bool is_shared_image, int8_t* name) {
  if (is_shared_image)
    name[kSharedImageFlagIndex] |= kSharedImageFlag;
  else
    name[kSharedImageFlagIndex] &= ~kSharedImageFlag;
}

Mailbox GenerateMailbox(bool is_shared_image) {
  Mailbox result;
  // Generates cryptographically-secure bytes.
  base::RandBytes(result.name, sizeof(result.name));
  MarkMailboxAsSharedImage(is_shared_image, result.name);
#if !defined(NDEBUG)
  int8_t value = 1;
  for (size_t i = 1; i < sizeof(result.name); ++i)
    value ^= result.name[i];
  result.name[0] = value;
#endif
  return result;
}

}  // namespace

Mailbox::Mailbox() {
  memset(name, 0, sizeof(name));
}

bool Mailbox::IsZero() const {
  for (size_t i = 0; i < std::size(name); ++i) {
    if (name[i])
      return false;
  }
  return true;
}

void Mailbox::SetZero() {
  memset(name, 0, sizeof(name));
}

void Mailbox::SetName(const int8_t* n) {
  DCHECK(IsZero() || !memcmp(name, n, sizeof(name)));
  memcpy(name, n, sizeof(name));
}

bool Mailbox::IsSharedImage() const {
  return name[kSharedImageFlagIndex] & kSharedImageFlag;
}

Mailbox Mailbox::GenerateLegacyMailbox() {
  return GenerateMailbox(false /* is_shared_image */);
}

Mailbox Mailbox::GenerateForSharedImage() {
  return GenerateMailbox(true /* is_shared_image */);
}

bool Mailbox::Verify() const {
#if !defined(NDEBUG)
  int8_t value = 1;
  for (size_t i = 0; i < sizeof(name); ++i)
    value ^= name[i];
  return value == 0;
#else
  return true;
#endif
}

std::string Mailbox::ToDebugString() const {
  std::string s;
  for (int i = 0; i < GL_MAILBOX_SIZE_CHROMIUM; ++i) {
    if (i > 0)
      s += ':';
    s += base::StringPrintf("%02X", static_cast<uint8_t>(name[i]));
  }
  return s;
}

}  // namespace gpu
