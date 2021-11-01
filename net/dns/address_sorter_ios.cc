// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/address_sorter.h"

#include <memory>

#include "base/notreached.h"

namespace net {

// static
std::unique_ptr<AddressSorter> AddressSorter::CreateAddressSorter() {
  // Not expected to ever need an AddressSorter on iOS.
  NOTREACHED();
  return nullptr;
}

}  // namespace net
