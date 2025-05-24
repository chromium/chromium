// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/message.h"

#include <string_view>

#include "base/check.h"
#include "base/numerics/safe_conversions.h"
#include "mojo/public/c/system/message_pipe.h"

namespace mojo {

MojoResult NotifyBadMessage(MessageHandle message,
                            const std::string_view& error) {
  DCHECK(message.is_valid());
  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(error.size()));
  return MojoNotifyBadMessage(message.value(), error.data(),
                              static_cast<uint32_t>(error.size()), nullptr);
}

}  // namespace mojo
