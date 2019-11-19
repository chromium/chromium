// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/mailbox_manager_sync.h"

#include <stddef.h>

#include <algorithm>

#include "base/containers/queue.h"
#include "base/synchronization/lock.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_implementation.h"

#if !defined(OS_MACOSX)
#include "ui/gl/gl_fence_egl.h"
#endif

namespace gpu {
namespace gles2 {

namespace {

base::LazyInstance<base::Lock>::DestructorAtExit g_lock =
    LAZY_INSTANCE_INITIALIZER;

#if !defined(OS_MACOSX)
typedef std::map<SyncToken, std::unique_ptr<gl::GLFence>> SyncTokenToFenceMap;
base::LazyInstance<SyncTokenToFenceMap>::DestructorAtExit
    g_sync_point_to_fence = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<base::queue<SyncTokenToFenceMap::iterator>>::DestructorAtExit
    g_sync_points = LAZY_INSTANCE_INITIALIZER;
#endif

void CreateFenceLocked(const SyncToken& sync_token) {
#if !defined(OS_MACOSX)
  g_lock.Get().AssertAcquired();
  if (gl::GetGLImplementation() == gl::kGLImplementationMockGL ||
      gl::GetGLImplementation() == gl::kGLImplementationStubGL)
    return;

  base::queue<SyncTokenToFenceMap::iterator>& sync_points = g_sync_points.Get();
  SyncTokenToFenceMap& sync_point_to_fence = g_sync_point_to_fence.Get();
  if (sync_token.release_count()) {
    while (!sync_points.empty() &&
           sync_points.front()->second->HasCompleted()) {
      sync_point_to_fence.erase(sync_points.front());
      sync_points.pop();
    }
    // Need to use EGL fences since we are likely not in a single share group.
    std::unique_ptr<gl::GLFence> fence = gl::GLFenceEGL::Create();
    if (!fence) {
      // Fall back to glFinish instead crashing such as in crbug.com/995376.
      LOG(ERROR) << "eglCreateSyncKHR failed";
      glFinish();
      return;
    }
    std::pair<SyncTokenToFenceMap::iterator, bool> result =
        sync_point_to_fence.insert(
            std::make_pair(sync_token, std::move(fence)));
    DCHECK(result.second);
    sync_points.push(result.first);
    DCHECK(sync_points.size() == sync_point_to_fence.size());
  }
#endif
}

void AcquireFenceLocked(const SyncToken& sync_token) {
#if !defined(OS_MACOSX)
  g_lock.Get().AssertAcquired();
  SyncTokenToFenceMap::iterator fence_it =
      g_sync_point_to_fence.Get().find(sync_token);
  if (fence_it != g_sync_point_to_fence.Get().end()) {
    fence_it->second->ServerWait();
  }
#endif
}

static const unsigned kNewTextureVersion = 1;

}  // anonymous namespace

base::LazyInstance<MailboxManagerSync::TextureGroup::MailboxToGroupMap>::
    DestructorAtExit MailboxManagerSync::TextureGroup::mailbox_to_group_ =
        LAZY_INSTANCE_INITIALIZER;

// static
MailboxManagerSync::TextureGroup* MailboxManagerSync::TextureGroup::FromName(
    const Mailbox& name) {
  MailboxToGroupMap::iterator it = mailbox_to_group_.Get().find(name);
  if (it == mailbox_to_group_.Get().end())
    return nullptr;

  return it->second.get();
}

MailboxManagerSync::TextureGroup::TextureGroup(
    const TextureDefinition& definition)
    : definition_(definition) {
}

MailboxManagerSync::TextureGroup::~TextureGroup() = default;

void MailboxManagerSync::TextureGroup::AddName(const Mailbox& name) {
  g_lock.Get().AssertAcquired();
  DCHECK(std::find(names_.begin(), names_.end(), name) == names_.end());
  names_.push_back(name);
  DCHECK(mailbox_to_group_.Get().find(name) == mailbox_to_group_.Get().end());
  mailbox_to_group_.Get()[name] = this;
}

void MailboxManagerSync::TextureGroup::RemoveName(const Mailbox& name) {
  g_lock.Get().AssertAcquired();
  std::vector<Mailbox>::iterator names_it =
      std::find(names_.begin(), names_.end(), name);
  DCHECK(names_it != names_.end());
  names_.erase(names_it);
  MailboxToGroupMap::iterator it = mailbox_to_group_.Get().find(name);
  DCHECK(it != mailbox_to_group_.Get().end());
  mailbox_to_group_.Get().erase(it);
}

void MailboxManagerSync::TextureGroup::AddTexture(MailboxManagerSync* manager,
                                                  Texture* texture) {
  g_lock.Get().AssertAcquired();
  DCHECK(std::find(textures_.begin(), textures_.end(),
                   std::make_pair(manager, texture)) == textures_.end());
  textures_.push_back(std::make_pair(manager, texture));
}

bool MailboxManagerSync::TextureGroup::RemoveTexture(
    MailboxManagerSync* manager,
    Texture* texture) {
  g_lock.Get().AssertAcquired();
  TextureGroup::TextureList::iterator tex_list_it = std::find(
      textures_.begin(), textures_.end(), std::make_pair(manager, texture));
  DCHECK(tex_list_it != textures_.end());
  if (textures_.size() == 1) {
    // This is the last texture so the group is going away.
    for (size_t n = 0; n < names_.size(); n++) {
      const Mailbox& name = names_[n];
      MailboxToGroupMap::iterator mbox_it =
          mailbox_to_group_.Get().find(name);
      DCHECK(mbox_it != mailbox_to_group_.Get().end());
      DCHECK(mbox_it->second.get() == this);
      mailbox_to_group_.Get().erase(mbox_it);
    }
    return false;
  } else {
    textures_.erase(tex_list_it);
    return true;
  }
}

Texture* MailboxManagerSync::TextureGroup::FindTexture(
    MailboxManagerSync* manager) {
  g_lock.Get().AssertAcquired();
  for (TextureGroup::TextureList::iterator it = textures_.begin();
       it != textures_.end(); it++) {
    if (it->first == manager)
      return it->second;
  }
  return nullptr;
}

MailboxManagerSync::TextureGroupRef::TextureGroupRef(unsigned version,
                                                     TextureGroup* group)
    : version(version), group(group) {
}

MailboxManagerSync::TextureGroupRef::TextureGroupRef(
    const TextureGroupRef& other) = default;

MailboxManagerSync::TextureGroupRef::~TextureGroupRef() = default;

MailboxManagerSync::MailboxManagerSync() = default;

MailboxManagerSync::~MailboxManagerSync() {
  DCHECK_EQ(0U, texture_to_group_.size());
}

// static
bool MailboxManagerSync::SkipTextureWorkarounds(const Texture* texture) {
  // Cannot support mips due to support mismatch between
  // EGL_KHR_gl_texture_2D_image and glEGLImageTargetTexture2DOES for
  // texture levels.
  bool has_mips = texture->NeedsMips() && texture->texture_complete();
  return texture->target() != GL_TEXTURE_2D || has_mips;
}

bool MailboxManagerSync::UsesSync() {
  return true;
}

Texture* MailboxManagerSync::ConsumeTexture(const Mailbox& mailbox) {
  base::AutoLock lock(g_lock.Get());
  // Relax the cross-thread access restriction to non-thread-safe RefCount.
  // The lock above protects non-thread-safe RefCount in TextureGroup.
  base::ScopedAllowCrossThreadRefCountAccess
      scoped_allow_cross_thread_ref_count_access;
  TextureGroup* group = TextureGroup::FromName(mailbox);
  if (!group)
    return nullptr;

  // Check if a texture already exists in this share group.
  Texture* texture = group->FindTexture(this);
  if (texture)
    return texture;

  // Otherwise create a new texture.
  texture = group->GetDefinition().CreateTexture();
  if (texture) {
    DCHECK(!SkipTextureWorkarounds(texture));
    texture->SetMailboxManager(this);
    group->AddTexture(this, texture);

    TextureGroupRef new_ref =
        TextureGroupRef(group->GetDefinition().version(), group);
    texture_to_group_.insert(std::make_pair(texture, new_ref));
  }

  return texture;
}

void MailboxManagerSync::ProduceTexture(const Mailbox& mailbox,
                                        TextureBase* texture_base) {
  DCHECK(texture_base);
  base::AutoLock lock(g_lock.Get());
  // Relax the cross-thread access restriction to non-thread-safe RefCount.
  // The lock above protects non-thread-safe RefCount in TextureGroup.
  base::ScopedAllowCrossThreadRefCountAccess
      scoped_allow_cross_thread_ref_count_access;
  if (TextureGroup::FromName(mailbox)) {
    DLOG(ERROR) << "Ignored attempt to reassign a mailbox";
    return;
  }

  Texture* texture = Texture::CheckedCast(texture_base);

  TextureToGroupMap::iterator tex_it = texture_to_group_.find(texture);
  TextureGroup* group_for_texture = nullptr;

  if (tex_it != texture_to_group_.end()) {
    group_for_texture = tex_it->second.group.get();
    DCHECK(group_for_texture);
  } else {
    // This is a new texture, so create a new group.
    texture->SetMailboxManager(this);
    TextureDefinition definition;
    if (!SkipTextureWorkarounds(texture)) {
      base::AutoUnlock unlock(g_lock.Get());
      definition = TextureDefinition(texture, kNewTextureVersion, nullptr);
    }
    group_for_texture = new TextureGroup(definition);
    group_for_texture->AddTexture(this, texture);
    texture_to_group_.insert(std::make_pair(
        texture, TextureGroupRef(kNewTextureVersion, group_for_texture)));
  }
  group_for_texture->AddName(mailbox);

  DCHECK(texture->mailbox_manager() == this);
}

void MailboxManagerSync::TextureDeleted(TextureBase* texture_base) {
  base::AutoLock lock(g_lock.Get());
  // Relax the cross-thread access restriction to non-thread-safe RefCount.
  // The lock above protects non-thread-safe RefCount in TextureGroup.
  base::ScopedAllowCrossThreadRefCountAccess
      scoped_allow_cross_thread_ref_count_access;

  Texture* texture = Texture::CheckedCast(texture_base);
  DCHECK(texture != nullptr);

  TextureToGroupMap::iterator tex_it = texture_to_group_.find(texture);
  DCHECK(tex_it != texture_to_group_.end());
  TextureGroup* group_for_texture = tex_it->second.group.get();
  if (group_for_texture->RemoveTexture(this, texture))
    UpdateDefinitionLocked(texture, &tex_it->second);
  texture_to_group_.erase(tex_it);
}

void MailboxManagerSync::UpdateDefinitionLocked(TextureBase* texture_base,
                                                TextureGroupRef* group_ref) {
  g_lock.Get().AssertAcquired();

  Texture* texture = Texture::CheckedCast(texture_base);
  DCHECK(texture != nullptr);

  if (SkipTextureWorkarounds(texture))
    return;

  gl::GLImage* image = texture->GetLevelImage(texture->target(), 0);
  TextureGroup* group = group_ref->group.get();
  const TextureDefinition& definition = group->GetDefinition();
  scoped_refptr<NativeImageBuffer> image_buffer = definition.image();

  // Make sure we don't clobber with an older version
  if (!definition.IsOlderThan(group_ref->version))
    return;

  // Also don't push redundant updates. Note that it would break the
  // versioning.
  if (definition.Matches(texture))
    return;

  // Don't try to push updates to texture that have a bound image (not created
  // by the MailboxManagerSync), as they were never shared to begin with.
  if (image && (!image_buffer || !image_buffer->IsClient(image)))
    return;

  group->SetDefinition(TextureDefinition(texture, ++group_ref->version,
                                         image ? image_buffer : nullptr));
}

void MailboxManagerSync::PushTextureUpdates(const SyncToken& token) {
  base::AutoLock lock(g_lock.Get());
  // Relax the cross-thread access restriction to non-thread-safe RefCount.
  // The lock above protects non-thread-safe RefCount in TextureGroup.
  base::ScopedAllowCrossThreadRefCountAccess
      scoped_allow_cross_thread_ref_count_access;

  for (TextureToGroupMap::iterator it = texture_to_group_.begin();
       it != texture_to_group_.end(); it++) {
    UpdateDefinitionLocked(it->first, &it->second);
  }
  CreateFenceLocked(token);
}

void MailboxManagerSync::PullTextureUpdates(const SyncToken& token) {
  using TextureUpdatePair = std::pair<Texture*, TextureDefinition>;
  std::vector<TextureUpdatePair> needs_update;
  {
    base::AutoLock lock(g_lock.Get());
    // Relax the cross-thread access restriction to non-thread-safe RefCount.
    // The lock above protects non-thread-safe RefCount in TextureGroup.
    base::ScopedAllowCrossThreadRefCountAccess
        scoped_allow_cross_thread_ref_count_access;
    AcquireFenceLocked(token);

    for (TextureToGroupMap::iterator it = texture_to_group_.begin();
         it != texture_to_group_.end(); it++) {
      const TextureDefinition& definition = it->second.group->GetDefinition();
      Texture* texture = it->first;
      unsigned& texture_version = it->second.version;
      if (texture_version == definition.version() ||
          definition.IsOlderThan(texture_version))
        continue;
      texture_version = definition.version();
      needs_update.push_back(TextureUpdatePair(texture, definition));
    }
  }

  if (!needs_update.empty()) {
    for (const TextureUpdatePair& pair : needs_update) {
      pair.second.UpdateTexture(pair.first);
    }
  }
}

}  // namespace gles2
}  // namespace gpu
