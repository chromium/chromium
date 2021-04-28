// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/identifiability_paint_op_digest.h"

#include <cstring>

#include "gpu/command_buffer/client/raster_interface.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkMatrix.h"

namespace blink {

namespace {

// To minimize performance impact, don't exceed kMaxDigestOps during the
// lifetime of this IdentifiabilityPaintOpDigest object.
constexpr int kMaxDigestOps = 1 << 20;

}  // namespace

// Storage for serialized PaintOp state.
Vector<char>& SerializationBuffer() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<Vector<char>>,
                                  serialization_buffer, ());
  return *serialization_buffer;
}

IdentifiabilityPaintOpDigest::IdentifiabilityPaintOpDigest(IntSize size)
    : IdentifiabilityPaintOpDigest(size, kMaxDigestOps) {}

IdentifiabilityPaintOpDigest::IdentifiabilityPaintOpDigest(IntSize size,
                                                           int max_digest_ops)
    : max_digest_ops_(max_digest_ops),
      size_(size),
      paint_cache_(cc::ClientPaintCache::kNoCachingBudget),
      serialize_options_(&image_provider_,
                         /*transfer_cache=*/nullptr,
                         &paint_cache_,
                         /*strike_server=*/nullptr,
                         /*color_space=*/nullptr,
                         /*can_use_lcd_text=*/false,
                         /*content_supports_distance_field_text=*/false,
                         /*max_texture_size=*/0) {
  serialize_options_.for_identifiability_study = true;
  constexpr size_t kInitialSize = 16 * 1024;
  if (IdentifiabilityStudySettings::Get()->IsTypeAllowed(
          blink::IdentifiableSurface::Type::kCanvasReadback) &&
      SerializationBuffer().size() < kInitialSize) {
    SerializationBuffer().resize(kInitialSize);
  }
}

IdentifiabilityPaintOpDigest::~IdentifiabilityPaintOpDigest() = default;

constexpr size_t IdentifiabilityPaintOpDigest::kInfiniteOps;

void IdentifiabilityPaintOpDigest::MaybeUpdateDigest(
    const sk_sp<const cc::PaintRecord>& paint_record,
    const size_t num_ops_to_visit) {
  if (!IdentifiabilityStudySettings::Get()->IsTypeAllowed(
          blink::IdentifiableSurface::Type::kCanvasReadback)) {
    return;
  }
  if (total_ops_digested_ >= max_digest_ops_) {
    encountered_skipped_ops_ = true;
    return;
  }

  // Determine how many PaintOps we'll need to digest after the initial digests
  // that are skipped.
  const size_t num_ops_to_digest = num_ops_to_visit - prefix_skip_count_;

  // The number of PaintOps digested in this MaybeUpdateDigest() call.
  size_t cur_ops_digested = 0;
  for (const auto* op : cc::PaintRecord::Iterator(paint_record.get())) {
    // Skip initial PaintOps that don't correspond to context operations.
    if (prefix_skip_count_ > 0) {
      prefix_skip_count_--;
      continue;
    }
    // Update the digest for at most |num_ops_to_digest| operations in this
    // MaybeUpdateDigest() invocation.
    if (num_ops_to_visit != kInfiniteOps &&
        cur_ops_digested >= num_ops_to_digest)
      break;

    // To capture font fallback identifiability, we capture text draw operations
    // at the 2D context layer. We still need to modify the token builder digest
    // since we want to track the relative ordering of text operations and other
    // operations.
    if (op->GetType() == cc::PaintOpType::DrawTextBlob) {
      constexpr uint64_t kDrawTextBlobValue =
          UINT64_C(0x8c1587a34065ea3b);  // Picked form a hat.
      builder_.AddValue(kDrawTextBlobValue);
      continue;
    }

    // DrawRecord PaintOps contain nested PaintOps.
    if (op->GetType() == cc::PaintOpType::DrawRecord) {
      const auto* draw_record_op = static_cast<const cc::DrawRecordOp*>(op);
      MaybeUpdateDigest(draw_record_op->record, kInfiniteOps);
      continue;
    }

    std::memset(SerializationBuffer().data(), 0, SerializationBuffer().size());
    size_t serialized_size;
    while ((serialized_size = op->Serialize(
                SerializationBuffer().data(), SerializationBuffer().size(),
                serialize_options_, nullptr, SkM44(), SkM44())) == 0) {
      constexpr size_t kMaxBufferSize =
          gpu::raster::RasterInterface::kDefaultMaxOpSizeHint << 2;
      if (SerializationBuffer().size() >= kMaxBufferSize) {
        encountered_skipped_ops_ = true;
        return;
      }
      SerializationBuffer().Grow(SerializationBuffer().size() << 1);
    }
    builder_.AddAtomic(base::as_bytes(
        base::make_span(SerializationBuffer().data(), serialized_size)));
    total_ops_digested_++;
    cur_ops_digested++;
  }
  DCHECK_EQ(prefix_skip_count_, 0u);
}

cc::ImageProvider::ScopedResult
IdentifiabilityPaintOpDigest::IdentifiabilityImageProvider::GetRasterContent(
    const cc::DrawImage& draw_image) {
  // TODO(crbug.com/973801): Compute digests on images.
  outer_->encountered_partially_digested_image_ = true;
  return ScopedResult();
}

}  // namespace blink
