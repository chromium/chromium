// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/multi_draw_manager.h"

#include <algorithm>
#include <ranges>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"

namespace gpu {
namespace gles2 {

MultiDrawManager::ResultData::ResultData() = default;
MultiDrawManager::ResultData::~ResultData() = default;

MultiDrawManager::ResultData::ResultData(ResultData&& rhs)
    : draw_function(rhs.draw_function),
      drawcount(rhs.drawcount),
      mode(rhs.mode),
      type(rhs.type),
      firsts(std::move(rhs.firsts)),
      counts(std::move(rhs.counts)),
      offsets(std::move(rhs.offsets)),
      indices(std::move(rhs.indices)),
      instance_counts(std::move(rhs.instance_counts)),
      basevertices(std::move(rhs.basevertices)),
      baseinstances(std::move(rhs.baseinstances)) {}

MultiDrawManager::ResultData& MultiDrawManager::ResultData::operator=(
    ResultData&& rhs) {
  if (&rhs == this) {
    return *this;
  }
  draw_function = rhs.draw_function;
  drawcount = rhs.drawcount;
  mode = rhs.mode;
  type = rhs.type;
  std::swap(firsts, rhs.firsts);
  std::swap(counts, rhs.counts);
  std::swap(offsets, rhs.offsets);
  std::swap(indices, rhs.indices);
  std::swap(instance_counts, rhs.instance_counts);
  std::swap(basevertices, rhs.basevertices);
  std::swap(baseinstances, rhs.baseinstances);
  return *this;
}

MultiDrawManager::MultiDrawManager(IndexStorageType index_type)
    : draw_state_(DrawState::End),
      current_draw_offset_(0),
      index_type_(index_type),
      result_() {}

bool MultiDrawManager::Begin(GLsizei drawcount) {
  if (draw_state_ != DrawState::End) {
    return false;
  }
  result_.drawcount = drawcount;
  current_draw_offset_ = 0;
  draw_state_ = DrawState::Begin;
  return true;
}

bool MultiDrawManager::End(ResultData* result) {
  DCHECK(result);
  if (draw_state_ != DrawState::Draw ||
      current_draw_offset_ != result_.drawcount) {
    return false;
  }
  draw_state_ = DrawState::End;
  *result = std::move(result_);
  return true;
}

bool MultiDrawManager::MultiDrawArrays(GLenum mode,
                                       base::span<const GLint> firsts,
                                       base::span<const GLsizei> counts) {
  CHECK(firsts.size() == counts.size());
  GLsizei drawcount;
  if (!base::CheckedNumeric<GLsizei>(firsts.size()).AssignIfValid(&drawcount)) {
    return false;
  }
  if (!EnsureDrawArraysFunction(DrawFunction::DrawArrays, mode, drawcount)) {
    return false;
  }
  CopyArraysHelper(drawcount, firsts, counts, {}, {}, {}, {});
  return true;
}

bool MultiDrawManager::MultiDrawArraysInstanced(
    GLenum mode,
    base::span<const GLint> firsts,
    base::span<const GLsizei> counts,
    base::span<const GLsizei> instance_counts) {
  CHECK(firsts.size() == counts.size());
  CHECK(firsts.size() == instance_counts.size());
  GLsizei drawcount;
  if (!base::CheckedNumeric<GLsizei>(firsts.size()).AssignIfValid(&drawcount)) {
    return false;
  }
  if (!EnsureDrawArraysFunction(DrawFunction::DrawArraysInstanced, mode,
                                drawcount)) {
    return false;
  }
  CopyArraysHelper(drawcount, firsts, counts, {}, instance_counts, {}, {});
  return true;
}

bool MultiDrawManager::MultiDrawArraysInstancedBaseInstance(
    GLenum mode,
    base::span<const GLint> firsts,
    base::span<const GLsizei> counts,
    base::span<const GLsizei> instance_counts,
    base::span<const GLuint> baseinstances) {
  CHECK(firsts.size() == counts.size());
  CHECK(firsts.size() == instance_counts.size());
  CHECK(firsts.size() == baseinstances.size());
  GLsizei drawcount;
  if (!base::CheckedNumeric<GLsizei>(firsts.size()).AssignIfValid(&drawcount)) {
    return false;
  }
  if (!EnsureDrawArraysFunction(DrawFunction::DrawArraysInstancedBaseInstance,
                                mode, drawcount)) {
    return false;
  }
  CopyArraysHelper(drawcount, firsts, counts, {}, instance_counts, {},
                   baseinstances);
  return true;
}

bool MultiDrawManager::MultiDrawElements(GLenum mode,
                                         base::span<const GLsizei> counts,
                                         GLenum type,
                                         base::span<const GLsizei> offsets) {
  CHECK(counts.size() == offsets.size());
  GLsizei drawcount;
  if (!base::CheckedNumeric<GLsizei>(counts.size()).AssignIfValid(&drawcount)) {
    return false;
  }
  if (!EnsureDrawElementsFunction(DrawFunction::DrawElements, mode, type,
                                  drawcount)) {
    return false;
  }
  CopyArraysHelper(drawcount, {}, counts, offsets, {}, {}, {});
  return true;
}

bool MultiDrawManager::MultiDrawElementsInstanced(
    GLenum mode,
    base::span<const GLsizei> counts,
    GLenum type,
    base::span<const GLsizei> offsets,
    base::span<const GLsizei> instance_counts) {
  CHECK(counts.size() == offsets.size());
  CHECK(counts.size() == instance_counts.size());
  GLsizei drawcount;
  if (!base::CheckedNumeric<GLsizei>(counts.size()).AssignIfValid(&drawcount)) {
    return false;
  }
  if (!EnsureDrawElementsFunction(DrawFunction::DrawElementsInstanced, mode,
                                  type, drawcount)) {
    return false;
  }
  CopyArraysHelper(drawcount, {}, counts, offsets, instance_counts, {}, {});
  return true;
}

bool MultiDrawManager::MultiDrawElementsInstancedBaseVertexBaseInstance(
    GLenum mode,
    base::span<const GLsizei> counts,
    GLenum type,
    base::span<const GLsizei> offsets,
    base::span<const GLsizei> instance_counts,
    base::span<const GLint> basevertices,
    base::span<const GLuint> baseinstances) {
  CHECK(counts.size() == offsets.size());
  CHECK(counts.size() == instance_counts.size());
  CHECK(counts.size() == basevertices.size());
  CHECK(counts.size() == baseinstances.size());
  GLsizei drawcount;
  if (!base::CheckedNumeric<GLsizei>(counts.size()).AssignIfValid(&drawcount)) {
    return false;
  }
  if (!EnsureDrawElementsFunction(
          DrawFunction::DrawElementsInstancedBaseVertexBaseInstance, mode, type,
          drawcount)) {
    return false;
  }
  CopyArraysHelper(drawcount, {}, counts, offsets, instance_counts,
                   basevertices, baseinstances);
  return true;
}

void MultiDrawManager::ResizeArrays() {
  switch (result_.draw_function) {
    case DrawFunction::DrawArraysInstancedBaseInstance:
      result_.baseinstances.resize(result_.drawcount);
      [[fallthrough]];
    case DrawFunction::DrawArraysInstanced:
      result_.instance_counts.resize(result_.drawcount);
      [[fallthrough]];
    case DrawFunction::DrawArrays:
      result_.firsts.resize(result_.drawcount);
      result_.counts.resize(result_.drawcount);
      break;
    case DrawFunction::DrawElementsInstancedBaseVertexBaseInstance:
      result_.basevertices.resize(result_.drawcount);
      result_.baseinstances.resize(result_.drawcount);
      [[fallthrough]];
    case DrawFunction::DrawElementsInstanced:
      result_.instance_counts.resize(result_.drawcount);
      [[fallthrough]];
    case DrawFunction::DrawElements:
      result_.counts.resize(result_.drawcount);
      switch (index_type_) {
        case IndexStorageType::Offset:
          result_.offsets.resize(result_.drawcount);
          break;
        case IndexStorageType::Pointer:
          result_.indices.resize(result_.drawcount);
          break;
      }
      break;
    default:
      NOTREACHED();
  }
}

bool MultiDrawManager::ValidateDrawcount(GLsizei drawcount) const {
  if (drawcount < 0) {
    return false;
  }
  GLsizei new_offset;
  if (!base::CheckAdd(current_draw_offset_, drawcount)
           .AssignIfValid(&new_offset)) {
    return false;
  }
  if (new_offset > result_.drawcount) {
    return false;
  }
  return true;
}

bool MultiDrawManager::EnsureDrawArraysFunction(DrawFunction draw_function,
                                                GLenum mode,
                                                GLsizei drawcount) {
  if (!ValidateDrawcount(drawcount)) {
    return false;
  }
  bool invalid_draw_state = draw_state_ == DrawState::End;
  bool first_call = draw_state_ == DrawState::Begin;
  bool enums_match = result_.mode == mode;

  if (invalid_draw_state || (!first_call && !enums_match)) {
    return false;
  }
  if (first_call) {
    draw_state_ = DrawState::Draw;
    result_.draw_function = draw_function;
    result_.mode = mode;
    ResizeArrays();
  }
  return true;
}

bool MultiDrawManager::EnsureDrawElementsFunction(DrawFunction draw_function,
                                                  GLenum mode,
                                                  GLenum type,
                                                  GLsizei drawcount) {
  if (!ValidateDrawcount(drawcount)) {
    return false;
  }
  bool invalid_draw_state = draw_state_ == DrawState::End;
  bool first_call = draw_state_ == DrawState::Begin;
  bool enums_match = result_.mode == mode && result_.type == type;

  if (invalid_draw_state || (!first_call && !enums_match)) {
    return false;
  }
  if (first_call) {
    draw_state_ = DrawState::Draw;
    result_.draw_function = draw_function;
    result_.mode = mode;
    result_.type = type;
    ResizeArrays();
  }
  return true;
}

void MultiDrawManager::CopyArraysHelper(
    GLsizei drawcount,
    base::span<const GLint> firsts,
    base::span<const GLsizei> counts,
    base::span<const GLsizei> offsets,
    base::span<const GLsizei> instance_counts,
    base::span<const GLint> basevertices,
    base::span<const GLuint> baseinstances) {
  size_t size = static_cast<size_t>(drawcount);
  size_t offset = static_cast<size_t>(current_draw_offset_);
  if (!firsts.empty()) {
    base::span(result_.firsts)
        .subspan(offset, size)
        .copy_from(firsts.first(size));
  }

  if (!counts.empty()) {
    base::span(result_.counts)
        .subspan(offset, size)
        .copy_from(counts.first(size));
  }

  if (!instance_counts.empty()) {
    base::span(result_.instance_counts)
        .subspan(offset, size)
        .copy_from(instance_counts.first(size));
  }

  if (!basevertices.empty()) {
    base::span(result_.basevertices)
        .subspan(offset, size)
        .copy_from(basevertices.first(size));
  }

  if (!baseinstances.empty()) {
    base::span(result_.baseinstances)
        .subspan(offset, size)
        .copy_from(baseinstances.first(size));
  }

  if (!offsets.empty()) {
    auto offsets_subspan = offsets.first(size);
    switch (index_type_) {
      case IndexStorageType::Offset:
        base::span(result_.offsets)
            .subspan(offset, size)
            .copy_from(offsets_subspan);
        break;
      case IndexStorageType::Pointer:
        std::ranges::transform(
            offsets_subspan,
            base::span(result_.indices).subspan(offset, size).begin(),
            [](uint32_t offset) {
              return reinterpret_cast<void*>(static_cast<intptr_t>(offset));
            });
        break;
    }
  }

  current_draw_offset_ += drawcount;
}

}  // namespace gles2
}  // namespace gpu
