// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/socket/packet.h"

#include <assert.h>
#include <string.h>

#include "nacl_io/pepper_interface.h"

namespace nacl_io {

Packet::Packet(PepperInterface* ppapi)
    : ppapi_(ppapi), addr_(0), buffer_(NULL), len_(0) {
}

Packet::~Packet() {
  if ((NULL != ppapi_) && addr_)
    ppapi_->ReleaseResource(addr_);
  free(buffer_);
}

void Packet::Copy(const void* buffer, size_t len, PP_Resource addr) {
  addr_ = addr;
  len_ = len;
  buffer_ = (char*)malloc(len);
  assert(buffer_);

  memcpy(buffer_, buffer, len);
  if (addr && (NULL != ppapi_))
    ppapi_->AddRefResource(addr);
}

}  // namespace nacl_io
