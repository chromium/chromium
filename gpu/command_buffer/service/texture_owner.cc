// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/texture_owner.h"

#include <memory>

#include "base/atomic_sequence_num.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "gpu/command_buffer/service/image_reader_gl_owner.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {
namespace {

// Generates process-unique IDs to use for tracing resources.
base::AtomicSequenceNumber g_next_texture_owner_tracing_id;

}  // namespace

TextureOwner::TextureOwner(scoped_refptr<SharedContextState> context_state)
    : base::RefCountedDeleteOnSequence<TextureOwner>(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      context_state_(std::move(context_state)),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      tracing_id_(g_next_texture_owner_tracing_id.GetNext()) {
  DCHECK(context_state_);
  context_state_->AddContextLostObserver(this);

  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "TextureOwner", base::SingleThreadTaskRunner::GetCurrentDefault());
}

TextureOwner::TextureOwner()
    : base::RefCountedDeleteOnSequence<TextureOwner>(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      tracing_id_(g_next_texture_owner_tracing_id.GetNext()) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "TextureOwner", base::SingleThreadTaskRunner::GetCurrentDefault());
}

TextureOwner::~TextureOwner() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);

  std::optional<ui::ScopedMakeCurrent> scoped_make_current;
  if (context_state_) {
    if (!context_state_->IsCurrent(nullptr, /*needs_gl=*/true)) {
      scoped_make_current.emplace(context_state_->context(),
                                  context_state_->surface());
      scoped_make_current->IsContextCurrent();
    }
    context_state_->RemoveContextLostObserver(this);
  }

  // Reset texture and context state here while the |context_state_| is current.
  context_state_.reset();
}

// static
scoped_refptr<TextureOwner> TextureOwner::Create(
    Mode mode,
    scoped_refptr<SharedContextState> context_state,
    scoped_refptr<RefCountedLock> drdc_lock,
    TextureOwnerCodecType type_for_metrics) {
  return new ImageReaderGLOwner(mode, std::move(context_state),
                                std::move(drdc_lock), type_for_metrics);
}

void TextureOwner::OnContextLost() {
  ReleaseResources();
  context_state_->RemoveContextLostObserver(this);
  context_state_.reset();
}

}  // namespace gpu
