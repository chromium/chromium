// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_factory.h"

#include <inttypes.h>

#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_backing_factory_gl_texture.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/wrapped_sk_image.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/gl/trace_util.h"

namespace gpu {

SharedImageFactory::SharedImageFactory(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    const GpuFeatureInfo& gpu_feature_info,
    raster::RasterDecoderContextState* context_state,
    MailboxManager* mailbox_manager,
    SharedImageManager* shared_image_manager,
    ImageFactory* image_factory,
    MemoryTracker* tracker)
    : mailbox_manager_(mailbox_manager),
      shared_image_manager_(shared_image_manager),
      backing_factory_(
          std::make_unique<SharedImageBackingFactoryGLTexture>(gpu_preferences,
                                                               workarounds,
                                                               gpu_feature_info,
                                                               image_factory,
                                                               tracker)),
      wrapped_sk_image_factory_(
          gpu_preferences.enable_raster_to_sk_image
              ? std::make_unique<raster::WrappedSkImageFactory>(context_state)
              : nullptr) {}

SharedImageFactory::~SharedImageFactory() {
  DCHECK(mailboxes_.empty());
}

bool SharedImageFactory::CreateSharedImage(const Mailbox& mailbox,
                                           viz::ResourceFormat format,
                                           const gfx::Size& size,
                                           const gfx::ColorSpace& color_space,
                                           uint32_t usage) {
  if (mailboxes_.find(mailbox) != mailboxes_.end()) {
    LOG(ERROR) << "CreateSharedImage: mailbox already exists";
    return false;
  }

  std::unique_ptr<SharedImageBacking> backing;
  bool using_wrapped_sk_image = wrapped_sk_image_factory_ &&
                                (usage & SHARED_IMAGE_USAGE_OOP_RASTERIZATION);
  if (using_wrapped_sk_image) {
    backing = wrapped_sk_image_factory_->CreateSharedImage(
        mailbox, format, size, color_space, usage);
  } else {
    backing = backing_factory_->CreateSharedImage(mailbox, format, size,
                                                  color_space, usage);
  }

  if (!backing) {
    LOG(ERROR) << "CreateSharedImage: could not create backing.";
    return false;
  }

  // TODO(ericrk): Handle the non-legacy case.
  if (!using_wrapped_sk_image &&
      !backing->ProduceLegacyMailbox(mailbox_manager_)) {
    LOG(ERROR)
        << "CreateSharedImage: could not convert backing to legacy mailbox.";
    backing->Destroy();
    return false;
  }

  if (!shared_image_manager_->Register(std::move(backing))) {
    LOG(ERROR) << "CreateSharedImage: Could not register backing with "
                  "SharedImageManager.";
    return false;
  }

  mailboxes_.emplace(mailbox);
  return true;
}

bool SharedImageFactory::DestroySharedImage(const Mailbox& mailbox) {
  auto it = mailboxes_.find(mailbox);
  if (it == mailboxes_.end()) {
    LOG(ERROR) << "Could not find shared image mailbox";
    return false;
  }
  shared_image_manager_->Unregister(mailbox);
  mailboxes_.erase(it);
  return true;
}

void SharedImageFactory::DestroyAllSharedImages(bool have_context) {
  for (const auto& mailbox : mailboxes_) {
    if (!have_context)
      shared_image_manager_->OnContextLost(mailbox);
    shared_image_manager_->Unregister(mailbox);
  }
  mailboxes_.clear();
}

bool SharedImageFactory::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd,
    int client_id,
    uint64_t client_tracing_id) {
  for (const auto& mailbox : mailboxes_) {
    shared_image_manager_->OnMemoryDump(mailbox, pmd, client_id,
                                        client_tracing_id);
  }

  return true;
}

}  // namespace gpu
