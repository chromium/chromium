// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/common/mailbox.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/check.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"

namespace gpu {
namespace {

// The last byte of the mailbox's name stores a bit that ensures that the
// mailbox doesn't end up being generated as zero. This avoids conflicts with
// Verify logic, which uses the first byte.
constexpr size_t kLiveMailboxIndex = GL_MAILBOX_SIZE_CHROMIUM - 1;

// Use the lowest bit for the flag marking the mailbox as live (any bit would
// work).
constexpr int8_t kLiveMailboxFlag = 0x1;

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

Mailbox Mailbox::Generate() {
  Mailbox result;
  // Generates cryptographically-secure bytes.
  base::RandBytes(base::as_writable_byte_span(result.name));

  // Ensure that the mailbox is non-zero.
  result.name[kLiveMailboxIndex] |= kLiveMailboxFlag;

#if !defined(NDEBUG)
  int8_t value = 1;
  for (size_t i = 1; i < sizeof(result.name); ++i) {
    value ^= result.name[i];
  }
  result.name[0] = value;
#endif
  return result;
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

bool Mailbox::operator==(const Mailbox& other) const {
  return memcmp(&name, &other.name, sizeof(name)) == 0;
}

std::strong_ordering Mailbox::operator<=>(const Mailbox& other) const {
  int result = memcmp(&name, &other.name, sizeof(name));
  if (result < 0) {
    return std::strong_ordering::less;
  } else if (result > 0) {
    return std::strong_ordering::greater;
  }
  return std::strong_ordering::equal;
}

}  // namespace gpu
