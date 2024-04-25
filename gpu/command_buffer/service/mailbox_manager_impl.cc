// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/mailbox_manager_impl.h"

#include <stddef.h>

#include <algorithm>

#include "base/check_op.h"
#include "base/logging.h"
#include "gpu/command_buffer/service/texture_base.h"

namespace gpu {
namespace gles2 {

MailboxManagerImpl::MailboxManagerImpl() = default;

MailboxManagerImpl::~MailboxManagerImpl() = default;

}  // namespace gles2
}  // namespace gpu
