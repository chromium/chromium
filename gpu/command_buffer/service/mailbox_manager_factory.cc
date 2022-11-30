// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/mailbox_manager_factory.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "gpu/command_buffer/service/mailbox_manager_dummy.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"

namespace gpu {
namespace gles2 {

std::unique_ptr<MailboxManager> CreateMailboxManager(
    const GpuPreferences& gpu_preferences) {
  // TODO(vikassoni):Once shared images have been completely tested and stable
  // on webview, remove MailboxManagerSync and MailboxManagerSyncDummy.
  if (gpu_preferences.enable_threaded_texture_mailboxes) {
    return std::make_unique<MailboxManagerDummy>();
  }
  return std::make_unique<MailboxManagerImpl>();
}

}  // namespace gles2
}  // namespace gpu
