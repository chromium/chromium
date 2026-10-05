// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/ift_custom_font_data.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/location.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/css/remote_font_face_source.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/resource/font_resource.h"
#include "third_party/blink/renderer/platform/fonts/ift/ift_patcher.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/code_point_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

IftCustomFontData* IftCustomFontData::MaybeCreate(
    RemoteFontFaceSource* font_face_source,
    FontResource& font_resource) {
  std::unique_ptr<IftPatcher> patcher = font_resource.TakeIftPatcher();
  if (!patcher) {
    return nullptr;
  }
  return MakeGarbageCollected<IftCustomFontData>(
      base::PassKey<IftCustomFontData>(), font_face_source, std::move(patcher));
}

IftCustomFontData::IftCustomFontData(base::PassKey<IftCustomFontData>,
                                     RemoteFontFaceSource* font_face_source,
                                     std::unique_ptr<IftPatcher> patcher)
    : font_face_source_(font_face_source),
      patcher_(std::move(patcher)),
      requested_subset_(std::make_unique<IftSubsetDefinition>()) {
  CHECK(font_face_source_);
  CHECK(patcher_);
}

IftCustomFontData::~IftCustomFontData() = default;

void IftCustomFontData::Trace(Visitor* visitor) const {
  visitor->Trace(font_face_source_);
  CustomFontData::Trace(visitor);
}

bool IftCustomFontData::IftRequireSubset(const StringView& text) const {
  bool subset_modified = false;
  for (UChar32 code_point : text) {
    subset_modified |=
        requested_subset_->AddCodepoint(static_cast<uint32_t>(code_point));
  }
  if (subset_modified) {
    BeginCollecting();
  }
  // TODO(wmedrano): Let's check directly with the actual font. Blocked on
  // https://github.com/googlefonts/fontations/issues/2173.
  return state_ == State::kIdle;
}

void IftCustomFontData::BeginCollecting() const {
  if (state_ != State::kIdle) {
    return;
  }
  state_ = State::kCollecting;
  ExecutionContext* execution_context =
      font_face_source_->GetExecutionContext();
  if (!execution_context) {
    BeginFailed();
    return;
  }
  execution_context->GetTaskRunner(TaskType::kFontLoading)
      ->PostTask(FROM_HERE, BindOnce(&IftCustomFontData::BeginFetching,
                                     WrapPersistent(this)));
}

void IftCustomFontData::BeginFetching() const {
  if (state_ == State::kFailed) {
    return;
  }
  state_ = State::kFetching;
}

void IftCustomFontData::BeginFailed() const {
  if (state_ == State::kFailed) {
    return;
  }
  state_ = State::kFailed;
}

}  // namespace blink
