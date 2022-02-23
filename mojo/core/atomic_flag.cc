// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/atomic_flag.h"

namespace mojo {
namespace core {

AtomicFlag::AtomicFlag(): flag_(false) {}

}  // namespace core
}  // namespace mojo
