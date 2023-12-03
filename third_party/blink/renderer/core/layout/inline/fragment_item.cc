// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"

#include "base/debug/dump_without_crashing.h"
#include "third_party/blink/renderer/core/editing/bidi_adjustment.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/inline/caret_position.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_items_builder.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_result.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/paint/inline_paint_context.h"
#include "third_party/blink/renderer/platform/fonts/text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

namespace {

struct SameSizeAsFragmentItem {
  union {
    FragmentItem::TextItem text_;
    FragmentItem::SvgTextItem svg_text_;
    FragmentItem::GeneratedTextItem generated_text_;
    FragmentItem::LineItem line_;
    FragmentItem::BoxItem box_;
  };
  PhysicalRect rect;
  InkOverflow ink_overflow;
  Member<void*> member;
  wtf_size_t sizes[2];
  unsigned flags;
};

ASSERT_SIZE(FragmentItem, SameSizeAsFragmentItem);

}  // namespace

FragmentItem::FragmentItem(const InlineItem& inline_item,
                           scoped_refptr<const ShapeResultView> shape_result,
                           const TextOffsetRange& text_offset,
                           const PhysicalSize& size,
                           bool is_hidden_for_paint)
    : text_({std::move(shape_result), text_offset}),
      rect_({PhysicalOffset(), size}),
      layout_object_(inline_item.GetLayoutObject()),
      const_traced_type_(kNone),
      type_(kText),
      sub_type_(static_cast<unsigned>(inline_item.TextType())),
      style_variant_(static_cast<unsigned>(inline_item.GetStyleVariant())),
      is_hidden_for_paint_(is_hidden_for_paint),
      text_direction_(static_cast<unsigned>(inline_item.Direction())),
      ink_overflow_type_(static_cast<unsigned>(InkOverflow::Type::kNotSet)),
      is_dirty_(false),
      is_last_for_node_(true) {
#if DCHECK_IS_ON()
  if (text_.shape_result) {
    DCHECK_EQ(text_.shape_result->StartIndex(), StartOffset());
    DCHECK_EQ(text_.shape_result->EndIndex(), EndOffset());
  }
#endif
  DCHECK_NE(TextType(), TextItemType::kLayoutGenerated);
  DCHECK(!IsFormattingContextRoot());
}

FragmentItem::FragmentItem(const LayoutObject& layout_object,
                           TextItemType text_type,
                           StyleVariant style_variant,
                           TextDirection direction,
                           scoped_refptr<const ShapeResultView> shape_result,
                           const String& text_content,
                           const PhysicalSize& size,
                           bool is_hidden_for_paint)
    : generated_text_({std::move(shape_result), text_content}),
      rect_({PhysicalOffset(), size}),
      layout_object_(&layout_object),
      const_traced_type_(kNone),
      type_(kGeneratedText),
      sub_type_(static_cast<unsigned>(text_type)),
      style_variant_(static_cast<unsigned>(style_variant)),
      is_hidden_for_paint_(is_hidden_for_paint),
      text_direction_(static_cast<unsigned>(direction)),
      ink_overflow_type_(static_cast<unsigned>(InkOverflow::Type::kNotSet)),
      is_dirty_(false),
      is_last_for_node_(true) {
  DCHECK(layout_object_);
  DCHECK_EQ(TextShapeResult()->StartIndex(), StartOffset());
  DCHECK_EQ(TextShapeResult()->EndIndex(), EndOffset());
  DCHECK(!IsFormattingContextRoot());
}

FragmentItem::FragmentItem(const InlineItem& inline_item,
                           scoped_refptr<const ShapeResultView> shape_result,
                           const String& text_content,
                           const PhysicalSize& size,
                           bool is_hidden_for_paint)
    : FragmentItem(*inline_item.GetLayoutObject(),
                   inline_item.TextType(),
                   inline_item.GetStyleVariant(),
                   inline_item.Direction(),
                   std::move(shape_result),
                   text_content,
                   size,
                   is_hidden_for_paint) {}

FragmentItem::FragmentItem(const PhysicalLineBoxFragment& line)
    : line_({&line, /* descendants_count */ 1}),
      rect_({PhysicalOffset(), line.Size()}),
      layout_object_(line.ContainerLayoutObject()),
      const_traced_type_(kLineItem),
      type_(kLine),
      sub_type_(static_cast<unsigned>(line.GetLineBoxType())),
      style_variant_(static_cast<unsigned>(line.GetStyleVariant())),
      is_hidden_for_paint_(false),
      text_direction_(static_cast<unsigned>(line.BaseDirection())),
      ink_overflow_type_(static_cast<unsigned>(InkOverflow::Type::kNotSet)),
      is_dirty_(false),
      is_last_for_node_(true) {
  DCHECK(!IsFormattingContextRoot());
}

FragmentItem::FragmentItem(const PhysicalBoxFragment& box,
                           TextDirection resolved_direction)
    : box_(&box, /* descendants_count */ 1),
      rect_({PhysicalOffset(), box.Size()}),
      layout_object_(box.GetLayoutObject()),
      const_traced_type_(kBoxItem),
      type_(kBox),
      style_variant_(static_cast<unsigned>(box.GetStyleVariant())),
      is_hidden_for_paint_(box.IsHiddenForPaint()),
      text_direction_(static_cast<unsigned>(resolved_direction)),
      ink_overflow_type_(static_cast<unsigned>(InkOverflow::Type::kNotSet)),
      is_dirty_(false),
      is_last_for_node_(true) {
  DCHECK_EQ(IsFormattingContextRoot(), box.IsFormattingContextRoot());
}

// |const_traced_type_| will be re-initialized in another constructor called
// inside this one.
FragmentItem::FragmentItem(LogicalLineItem&& line_item,
                           WritingMode writing_mode)
    : const_traced_type_(kNone) {
  DCHECK(line_item.CanCreateFragmentItem());

  if (line_item.inline_item) {
    if (UNLIKELY(line_item.text_content)) {
      new (this) FragmentItem(
          *line_item.inline_item, std::move(line_item.shape_result),
          line_item.text_content,
          ToPhysicalSize(line_item.MarginSize(), writing_mode),
          line_item.is_hidden_for_paint);
      return;
    }

    new (this)
        FragmentItem(*line_item.inline_item, std::move(line_item.shape_result),
                     line_item.text_offset,
                     ToPhysicalSize(line_item.MarginSize(), writing_mode),
                     line_item.is_hidden_for_paint);
    return;
  }

  if (line_item.layout_result) {
    const auto& box_fragment =
        To<PhysicalBoxFragment>(line_item.layout_result->GetPhysicalFragment());
    new (this) FragmentItem(box_fragment, line_item.ResolvedDirection());
    return;
  }

  if (line_item.layout_object) {
    const TextDirection direction = line_item.shape_result->Direction();
    new (this)
        FragmentItem(*line_item.layout_object, TextItemType::kLayoutGenerated,
                     line_item.style_variant, direction,
                     std::move(line_item.shape_result), line_item.text_content,
                     ToPhysicalSize(line_item.MarginSize(), writing_mode),
                     line_item.is_hidden_for_paint);
    return;
  }

  // CanCreateFragmentItem()
  NOTREACHED();
  CHECK(false);
}

FragmentItem::FragmentItem(const FragmentItem& source)
    : rect_(source.rect_),
      layout_object_(source.layout_object_),
      fragment_id_(source.fragment_id_),
      delta_to_next_for_same_layout_object_(
          source.delta_to_next_for_same_layout_object_),
      const_traced_type_(source.const_traced_type_),
      type_(source.type_),
      sub_type_(source.sub_type_),
      style_variant_(source.style_variant_),
      is_hidden_for_paint_(source.is_hidden_for_paint_),
      text_direction_(source.text_direction_),
      ink_overflow_type_(static_cast<unsigned>(InkOverflow::Type::kNotSet)),
      is_dirty_(source.is_dirty_),
      is_last_for_node_(source.is_last_for_node_) {
  switch (Type()) {
    case kInvalid:
      NOTREACHED_NORETURN() << "Cannot construct invalid value";
    case kText:
      new (&text_) TextItem(source.text_);
      break;
    case kSvgText:
      new (&svg_text_) SvgTextItem();
      svg_text_.data =
          std::make_unique<SvgFragmentData>(*source.svg_text_.data);
      break;
    case kGeneratedText:
      new (&generated_text_) GeneratedTextItem(source.generated_text_);
      break;
    case kLine:
      new (&line_) LineItem(source.line_);
      break;
    case kBox:
      new (&box_) BoxItem(source.box_);
      break;
  }

  if (source.IsInkOverflowComputed()) {
    ink_overflow_type_ = static_cast<unsigned>(source.InkOverflowType());
    new (&ink_overflow_)
        InkOverflow(source.InkOverflowType(), source.ink_overflow_);
  }
}

FragmentItem::FragmentItem(FragmentItem&& source)
    : rect_(source.rect_),
      ink_overflow_(source.InkOverflowType(), std::move(source.ink_overflow_)),
      layout_object_(source.layout_object_),
      fragment_id_(source.fragment_id_),
      delta_to_next_for_same_layout_object_(
          source.delta_to_next_for_same_layout_object_),
      const_traced_type_(source.const_traced_type_),
      type_(source.type_),
      sub_type_(source.sub_type_),
      style_variant_(source.style_variant_),
      is_hidden_for_paint_(source.is_hidden_for_paint_),
      text_direction_(source.text_direction_),
      ink_overflow_type_(source.ink_overflow_type_),
      is_dirty_(source.is_dirty_),
      is_last_for_node_(source.is_last_for_node_) {
  switch (Type()) {
    case kInvalid:
      NOTREACHED_NORETURN() << "Cannot construct invalid value";
    case kText:
      new (&text_) TextItem(std::move(source.text_));
      break;
    case kSvgText:
      new (&svg_text_) SvgTextItem(std::move(source.svg_text_));
      break;
    case kGeneratedText:
      new (&generated_text_)
          GeneratedTextItem(std::move(source.generated_text_));
      break;
    case kLine:
      new (&line_) LineItem(std::move(source.line_));
      break;
    case kBox:
      new (&box_) BoxItem(std::move(source.box_));
      break;
  }
}

FragmentItem::~FragmentItem() {
  switch (Type()) {
    case kInvalid:
      // Slot can be zeroed, do nothing.
      return;
    case kText:
      text_.~TextItem();
      break;
    case kSvgText:
      svg_text_.~SvgTextItem();
      break;
    case kGeneratedText:
      generated_text_.~GeneratedTextItem();
      break;
    case kLine:
      line_.~LineItem();
      break;
    case kBox:
      box_.~BoxItem();
      break;
  }
  ink_overflow_.Reset(InkOverflowType());
}

bool FragmentItem::IsInlineBox() const {
  if (Type() == kBox) {
    if (const PhysicalBoxFragment* box = BoxFragment()) {
      return box->IsInlineBox();
    }
    NOTREACHED();
  }
  return false;
}

bool FragmentItem::IsAtomicInline() const {
  if (Type() != kBox)
    return false;
  if (const PhysicalBoxFragment* box = BoxFragment()) {
    return box->IsAtomicInline();
  }
  return false;
}

bool FragmentItem::IsBlockInInline() const {
  switch (Type()) {
    case kBox:
      if (auto* box = BoxFragment())
        return box->IsBlockInInline();
      return false;
    case kLine:
      if (auto* line_box = LineBoxFragment())
        return line_box->IsBlockInInline();
      return false;
    default:
      return false;
  }
}

bool FragmentItem::IsFloating() const {
  if (const PhysicalBoxFragment* box = BoxFragment()) {
    return box->IsFloating();
  }
  return false;
}

bool FragmentItem::IsEmptyLineBox() const {
  return GetLineBoxType() == LineBoxType::kEmptyLineBox;
}

bool FragmentItem::IsStyleGeneratedText() const {
  if (Type() == kText || Type() == kSvgText)
    return GetLayoutObject()->IsStyleGenerated();
  return false;
}

bool FragmentItem::IsGeneratedText() const {
  return IsLayoutGeneratedText() || IsStyleGeneratedText();
}

bool FragmentItem::IsFormattingContextRoot() const {
  const PhysicalBoxFragment* box = BoxFragment();
  return box && box->IsFormattingContextRoot();
}

bool FragmentItem::IsListMarker() const {
  return layout_object_ && layout_object_->IsLayoutOutsideListMarker();
}

LayoutObject& FragmentItem::BlockInInline() const {
  DCHECK(IsBlockInInline());
  auto* const block = To<LayoutNGBlockFlow>(GetLayoutObject())->FirstChild();
  DCHECK(block) << this;
  return *block;
}

void FragmentItem::ConvertToSvgText(std::unique_ptr<SvgFragmentData> data,
                                    const PhysicalRect& unscaled_rect,
                                    bool is_hidden) {
  DCHECK_EQ(Type(), kText);
  is_hidden_for_paint_ = is_hidden;
  text_.~TextItem();
  new (&svg_text_) SvgTextItem();
  svg_text_.data = std::move(data);
  type_ = kSvgText;
  rect_ = unscaled_rect;
}

void FragmentItem::SetSvgLineLocalRect(const PhysicalRect& unscaled_rect) {
  DCHECK_EQ(Type(), kLine);
  rect_ = unscaled_rect;
}

gfx::RectF FragmentItem::ObjectBoundingBox(const FragmentItems& items) const {
  DCHECK_EQ(Type(), kSvgText);
  const Font& scaled_font = ScaledFont();
  gfx::RectF ink_bounds = scaled_font.TextInkBounds(TextPaintInfo(items));
  if (const auto* font_data = scaled_font.PrimaryFont())
    ink_bounds.Offset(0.0f, font_data->GetFontMetrics().FloatAscent());
  ink_bounds.Scale(GetSvgFragmentData()->length_adjust_scale, 1.0f);
  const gfx::RectF& scaled_rect = GetSvgFragmentData()->rect;
  if (!IsHorizontal()) {
    ink_bounds =
        gfx::RectF(scaled_rect.width() - ink_bounds.bottom(), ink_bounds.x(),
                   ink_bounds.height(), ink_bounds.width());
  }
  ink_bounds.Offset(scaled_rect.OffsetFromOrigin());
  ink_bounds.Union(scaled_rect);
  if (HasSvgTransformForBoundingBox())
    ink_bounds = BuildSvgTransformForBoundingBox().MapRect(ink_bounds);
  ink_bounds.Scale(1 / SvgScalingFactor());
  return ink_bounds;
}

gfx::QuadF FragmentItem::SvgUnscaledQuad() const {
  DCHECK_EQ(Type(), kSvgText);
  gfx::QuadF quad = BuildSvgTransformForBoundingBox().MapQuad(
      gfx::QuadF(GetSvgFragmentData()->rect));
  const float scaling_factor = SvgScalingFactor();
  quad.Scale(1 / scaling_factor, 1 / scaling_factor);
  return quad;
}

PhysicalOffset FragmentItem::MapPointInContainer(
    const PhysicalOffset& point) const {
  if (Type() != kSvgText || !HasSvgTransformForBoundingBox())
    return point;
  const float scaling_factor = SvgScalingFactor();
  return PhysicalOffset::FromPointFRound(
      gfx::ScalePoint(BuildSvgTransformForBoundingBox().Inverse().MapPoint(
                          gfx::ScalePoint(gfx::PointF(point), scaling_factor)),
                      scaling_factor));
}

float FragmentItem::ScaleInlineOffset(LayoutUnit inline_offset) const {
  if (Type() != kSvgText)
    return inline_offset.ToFloat();
  return inline_offset.ToFloat() * SvgScalingFactor() /
         GetSvgFragmentData()->length_adjust_scale;
}

bool FragmentItem::InclusiveContains(const gfx::PointF& position) const {
  DCHECK_EQ(Type(), kSvgText);
  gfx::PointF scaled_position = gfx::ScalePoint(position, SvgScalingFactor());
  const gfx::RectF& item_rect = GetSvgFragmentData()->rect;
  if (!HasSvgTransformForBoundingBox())
    return item_rect.InclusiveContains(scaled_position);
  return BuildSvgTransformForBoundingBox()
      .MapQuad(gfx::QuadF(item_rect))
      .Contains(scaled_position);
}

bool FragmentItem::HasNonVisibleOverflow() const {
  if (const PhysicalBoxFragment* fragment = BoxFragment()) {
    return fragment->HasNonVisibleOverflow();
  }
  return false;
}

bool FragmentItem::IsScrollContainer() const {
  if (const PhysicalBoxFragment* fragment = BoxFragment()) {
    return fragment->IsScrollContainer();
  }
  return false;
}

bool FragmentItem::HasSelfPaintingLayer() const {
  if (const PhysicalBoxFragment* fragment = BoxFragment()) {
    return fragment->HasSelfPaintingLayer();
  }
  return false;
}

FragmentItem::BoxItem::BoxItem(const PhysicalBoxFragment* box_fragment,
                               wtf_size_t descendants_count)
    : box_fragment(box_fragment), descendants_count(descendants_count) {}

void FragmentItem::BoxItem::Trace(Visitor* visitor) const {
  visitor->Trace(box_fragment);
}

const PhysicalBoxFragment* FragmentItem::BoxItem::PostLayout() const {
  if (box_fragment)
    return box_fragment->PostLayout();
  return nullptr;
}

void FragmentItem::LayoutObjectWillBeDestroyed() const {
  const_cast<FragmentItem*>(this)->layout_object_ = nullptr;
  if (const PhysicalBoxFragment* fragment = BoxFragment()) {
    fragment->LayoutObjectWillBeDestroyed();
  }
}

void FragmentItem::LayoutObjectWillBeMoved() const {
  // When |Layoutobject| is moved out from the current IFC, we should not clear
  // the association with it in |ClearAssociatedFragments|, because the
  // |LayoutObject| may be moved to a different IFC and is already laid out
  // before clearing this IFC. This happens e.g., when split inlines moves
  // inline children into a child anonymous block.
  const_cast<FragmentItem*>(this)->layout_object_ = nullptr;
}

const PhysicalOffset FragmentItem::ContentOffsetInContainerFragment() const {
  PhysicalOffset offset = OffsetInContainerFragment();
  if (const PhysicalBoxFragment* box = BoxFragment()) {
    offset += box->ContentOffset();
  }
  return offset;
}

inline const LayoutBox* FragmentItem::InkOverflowOwnerBox() const {
  if (Type() == kBox)
    return DynamicTo<LayoutBox>(GetLayoutObject());
  return nullptr;
}

inline LayoutBox* FragmentItem::MutableInkOverflowOwnerBox() {
  if (Type() == kBox) {
    return DynamicTo<LayoutBox>(
        const_cast<LayoutObject*>(layout_object_.Get()));
  }
  return nullptr;
}

PhysicalRect FragmentItem::SelfInkOverflowRect() const {
  if (const PhysicalBoxFragment* box_fragment = BoxFragment()) {
    return box_fragment->SelfInkOverflowRect();
  }
  if (!HasInkOverflow())
    return LocalRect();
  return ink_overflow_.Self(InkOverflowType(), Size());
}

PhysicalRect FragmentItem::InkOverflowRect() const {
  if (const PhysicalBoxFragment* box_fragment = BoxFragment()) {
    return box_fragment->InkOverflowRect();
  }
  if (!HasInkOverflow())
    return LocalRect();
  if (!IsContainer() || HasNonVisibleOverflow())
    return ink_overflow_.Self(InkOverflowType(), Size());
  return ink_overflow_.SelfAndContents(InkOverflowType(), Size());
}

const ShapeResultView* FragmentItem::TextShapeResult() const {
  if (Type() == kText)
    return text_.shape_result.get();
  if (Type() == kSvgText)
    return svg_text_.data->shape_result.get();
  if (Type() == kGeneratedText)
    return generated_text_.shape_result.get();
  NOTREACHED();
  return nullptr;
}

TextOffsetRange FragmentItem::TextOffset() const {
  if (Type() == kText)
    return text_.text_offset;
  if (Type() == kSvgText)
    return svg_text_.data->text_offset;
  if (Type() == kGeneratedText)
    return {0, generated_text_.text.length()};
  NOTREACHED();
  return {};
}

unsigned FragmentItem::StartOffsetInContainer(
    const InlineCursor& container) const {
  DCHECK_EQ(Type(), kGeneratedText);
  DCHECK(!IsEllipsis());
  // Hyphens don't have the text offset in the container. Find the closest
  // previous text fragment.
  DCHECK_EQ(container.Current().Item(), this);
  InlineCursor cursor(container);
  for (cursor.MoveToPrevious(); cursor; cursor.MoveToPrevious()) {
    const InlineCursorPosition& current = cursor.Current();
    if (current->IsText() && !current->IsLayoutGeneratedText())
      return current->EndOffset();
    // A box doesn't have the offset either.
    if (current->Type() == kBox && !current->IsInlineBox())
      break;
  }
  NOTREACHED();
  return 0;
}

StringView FragmentItem::Text(const FragmentItems& items) const {
  if (Type() == kText) {
    return StringView(items.Text(UsesFirstLineStyle()), text_.text_offset.start,
                      text_.text_offset.Length());
  }
  if (Type() == kSvgText) {
    return StringView(items.Text(UsesFirstLineStyle()),
                      svg_text_.data->text_offset.start,
                      svg_text_.data->text_offset.Length());
  }
  if (Type() == kGeneratedText)
    return GeneratedText();
  NOTREACHED();
  return StringView();
}

TextFragmentPaintInfo FragmentItem::TextPaintInfo(
    const FragmentItems& items) const {
  if (Type() == kText) {
    return {items.Text(UsesFirstLineStyle()), text_.text_offset.start,
            text_.text_offset.end, text_.shape_result.get()};
  }
  if (Type() == kSvgText) {
    return {items.Text(UsesFirstLineStyle()), svg_text_.data->text_offset.start,
            svg_text_.data->text_offset.end,
            svg_text_.data->shape_result.get()};
  }
  if (Type() == kGeneratedText) {
    return {generated_text_.text, 0, generated_text_.text.length(),
            generated_text_.shape_result.get()};
  }
  NOTREACHED();
  return {};
}

TextDirection FragmentItem::BaseDirection() const {
  DCHECK_EQ(Type(), kLine);
  return static_cast<TextDirection>(text_direction_);
}

TextDirection FragmentItem::ResolvedDirection() const {
  DCHECK(IsText() || IsAtomicInline());
  return static_cast<TextDirection>(text_direction_);
}

bool FragmentItem::HasSvgTransformForPaint() const {
  return Type() == kSvgText && (svg_text_.data->length_adjust_scale != 1.0f ||
                                svg_text_.data->angle != 0.0f);
}

bool FragmentItem::HasSvgTransformForBoundingBox() const {
  return Type() == kSvgText && svg_text_.data->angle != 0.0f;
}

// For non-<textPath>:
//   length-adjust * translate(x, y) * rotate() * translate(-x, -y)
// For <textPath>:
//   translate(x, y) * rotate() * length-adjust * translate(-x, -y)
//
// (x, y) is the center of the rotation.  The center points of a non-<textPath>
// character and a <textPath> character are different.
AffineTransform FragmentItem::BuildSvgTransformForPaint() const {
  DCHECK_EQ(Type(), kSvgText);
  if (svg_text_.data->in_text_path) {
    if (svg_text_.data->angle == 0.0f)
      return BuildSvgTransformForLengthAdjust();
    return BuildSvgTransformForTextPath(BuildSvgTransformForLengthAdjust());
  }
  AffineTransform transform = BuildSvgTransformForBoundingBox();
  AffineTransform length_adjust = BuildSvgTransformForLengthAdjust();
  if (!length_adjust.IsIdentity())
    transform.PostConcat(length_adjust);
  return transform;
}

AffineTransform FragmentItem::BuildSvgTransformForLengthAdjust() const {
  DCHECK_EQ(Type(), kSvgText);
  const SvgFragmentData& svg_data = *svg_text_.data;
  const bool is_horizontal = IsHorizontal();
  AffineTransform scale_transform;
  float scale = svg_data.length_adjust_scale;
  if (scale != 1.0f) {
    // Inline offset adjustment is not necessary if this works with textPath
    // rotation.
    const bool with_text_path_transform =
        svg_data.in_text_path && svg_data.angle != 0.0f;
    // We'd like to scale only inline-size without moving inline position.
    if (is_horizontal) {
      float x = svg_data.rect.x();
      scale_transform.SetMatrix(
          scale, 0, 0, 1, with_text_path_transform ? 0 : x - scale * x, 0);
    } else {
      float y = svg_data.rect.y();
      scale_transform.SetMatrix(1, 0, 0, scale, 0,
                                with_text_path_transform ? 0 : y - scale * y);
    }
  }
  return scale_transform;
}

AffineTransform FragmentItem::BuildSvgTransformForTextPath(
    const AffineTransform& length_adjust) const {
  DCHECK_EQ(Type(), kSvgText);
  const SvgFragmentData& svg_data = *svg_text_.data;
  DCHECK(svg_data.in_text_path);
  DCHECK_NE(svg_data.angle, 0.0f);

  AffineTransform transform;
  transform.Rotate(svg_data.angle);

  const SimpleFontData* font_data = ScaledFont().PrimaryFont();

  // https://svgwg.org/svg2-draft/text.html#TextpathLayoutRules
  // The rotation should be about the center of the baseline.
  const auto font_baseline = Style().GetFontBaseline();
  // |x| in the horizontal writing-mode and |y| in the vertical writing-mode
  // point the center of the baseline.  See |SvgTextLayoutAlgorithm::
  // PositionOnPath()|.
  float x = svg_data.rect.x();
  float y = svg_data.rect.y();
  if (IsHorizontal()) {
    y += font_data->GetFontMetrics().FixedAscent(font_baseline);
    transform.Translate(-svg_data.rect.width() / 2, svg_data.baseline_shift);
  } else {
    x += font_data->GetFontMetrics().FixedDescent(font_baseline);
    transform.Translate(svg_data.baseline_shift, -svg_data.rect.height() / 2);
  }
  transform.PreConcat(length_adjust);
  transform.SetE(transform.E() + x);
  transform.SetF(transform.F() + y);
  transform.Translate(-x, -y);
  return transform;
}

// This function returns:
//   translate(x, y) * rotate() * translate(-x, -y)
//
// (x, y) is the center of the rotation.  The center points of a non-<textPath>
// character and a <textPath> character are different.
AffineTransform FragmentItem::BuildSvgTransformForBoundingBox() const {
  DCHECK_EQ(Type(), kSvgText);
  const SvgFragmentData& svg_data = *svg_text_.data;
  AffineTransform transform;
  if (svg_data.angle == 0.0f)
    return transform;
  if (svg_data.in_text_path)
    return BuildSvgTransformForTextPath(AffineTransform());

  transform.Rotate(svg_data.angle);
  const SimpleFontData* font_data = ScaledFont().PrimaryFont();
  // https://svgwg.org/svg2-draft/text.html#TextElementRotateAttribute
  // > The supplemental rotation, in degrees, about the current text position
  //
  // TODO(crbug.com/1179585): The following code is equivalent to the legacy
  // SVG. That is to say, rotation around the left edge of the baseline.
  // However it doesn't look correct for RTL and vertical text.
  float ascent =
      font_data ? font_data->GetFontMetrics().FixedAscent().ToFloat() : 0.0f;
  float y = svg_data.rect.y() + ascent;
  transform.SetE(transform.E() + svg_data.rect.x());
  transform.SetF(transform.F() + y);
  transform.Translate(-svg_data.rect.x(), -y);
  return transform;
}

float FragmentItem::SvgScalingFactor() const {
  const auto* svg_inline_text =
      DynamicTo<LayoutSVGInlineText>(GetLayoutObject());
  if (!svg_inline_text)
    return 1.0f;
  const float scaling_factor = svg_inline_text->ScalingFactor();
  DCHECK_GT(scaling_factor, 0.0f);
  return scaling_factor;
}

const Font& FragmentItem::ScaledFont() const {
  if (const auto* svg_inline_text =
          DynamicTo<LayoutSVGInlineText>(GetLayoutObject()))
    return svg_inline_text->ScaledFont();
  return Style().GetFont();
}

String FragmentItem::ToString() const {
  // TODO(yosin): Once |NGPaintFragment| is removed, we should get rid of
  // following if-statements.
  // For ease of rebasing, we use same |DebugName()| as |NGPaintFrgment|.
  if (Type() == FragmentItem::kBox) {
    StringBuilder name;
    name.Append("FragmentItem Box ");
    name.Append(layout_object_->DebugName());
    return name.ToString();
  }
  if (Type() == FragmentItem::kText) {
    StringBuilder name;
    name.Append("FragmentItem Text ");
    const FragmentItems* fragment_items = nullptr;
    if (const LayoutBlockFlow* block_flow =
            layout_object_->FragmentItemsContainer()) {
      for (unsigned i = 0; i < block_flow->PhysicalFragmentCount(); ++i) {
        const PhysicalBoxFragment* containing_fragment =
            block_flow->GetPhysicalFragment(i);
        fragment_items = containing_fragment->Items();
        if (fragment_items)
          break;
      }
    }
    if (fragment_items)
      name.Append(Text(*fragment_items).ToString().EncodeForDebugging());
    else
      name.Append("\"(container not found)\"");
    return name.ToString();
  }
  if (Type() == FragmentItem::kLine) {
    return "FragmentItem Line";
  }
  return "FragmentItem";
}

PhysicalRect FragmentItem::LocalVisualRectFor(
    const LayoutObject& layout_object) {
  DCHECK(layout_object.IsInLayoutNGInlineFormattingContext());

  PhysicalRect visual_rect;
  InlineCursor cursor;
  for (cursor.MoveTo(layout_object); cursor;
       cursor.MoveToNextForSameLayoutObject()) {
    DCHECK(cursor.Current().Item());
    const FragmentItem& item = *cursor.Current().Item();
    if (UNLIKELY(item.IsHiddenForPaint()))
      continue;
    PhysicalRect child_visual_rect = item.SelfInkOverflowRect();
    child_visual_rect.offset += item.OffsetInContainerFragment();
    visual_rect.Unite(child_visual_rect);
  }
  return visual_rect;
}

void FragmentItem::InvalidateInkOverflow() {
  ink_overflow_type_ =
      static_cast<unsigned>(ink_overflow_.Invalidate(InkOverflowType()));
}

PhysicalRect FragmentItem::RecalcInkOverflowForCursor(
    InlineCursor* cursor,
    InlinePaintContext* inline_context) {
  DCHECK(cursor);
  DCHECK(!cursor->Current() || cursor->IsAtFirst());
  PhysicalRect contents_ink_overflow;
  for (; *cursor; cursor->MoveToNextSkippingChildren()) {
    const FragmentItem* item = cursor->CurrentItem();
    DCHECK(item);
    if (UNLIKELY(item->IsLayoutObjectDestroyedOrMoved())) {
      // TODO(crbug.com/1099613): This should not happen, as long as it is
      // layout-clean. It looks like there are cases where the layout is dirty.
      continue;
    }
    if (UNLIKELY(item->HasSelfPaintingLayer()))
      continue;

    PhysicalRect child_rect;
    item->GetMutableForPainting().RecalcInkOverflow(*cursor, inline_context,
                                                    &child_rect);
    if (!child_rect.IsEmpty()) {
      child_rect.offset += item->OffsetInContainerFragment();
      contents_ink_overflow.Unite(child_rect);
    }
  }
  return contents_ink_overflow;
}

void FragmentItem::RecalcInkOverflow(const InlineCursor& cursor,
                                     InlinePaintContext* inline_context,
                                     PhysicalRect* self_and_contents_rect_out) {
  DCHECK_EQ(this, cursor.CurrentItem());

  if (UNLIKELY(IsLayoutObjectDestroyedOrMoved())) {
    // TODO(crbug.com/1099613): This should not happen, as long as it is really
    // layout-clean. It looks like there are cases where the layout is dirty.
    NOTREACHED();
    return;
  }

  if (IsText()) {
    // Re-computing text item is not necessary, because all changes that needs
    // to re-compute ink overflow invalidate layout. Except for box shadows,
    // text decorations and outlines that are invalidated before this point in
    // the code.
    if (IsInkOverflowComputed()) {
      *self_and_contents_rect_out = SelfInkOverflowRect();
      return;
    }

    TextFragmentPaintInfo paint_info = TextPaintInfo(cursor.Items());
    if (paint_info.shape_result) {
      if (Type() == kSvgText) {
        ink_overflow_type_ =
            static_cast<unsigned>(ink_overflow_.SetSvgTextInkOverflow(
                InkOverflowType(), cursor, paint_info, Style(), ScaledFont(),
                GetSvgFragmentData()->rect, SvgScalingFactor(),
                GetSvgFragmentData()->length_adjust_scale,
                BuildSvgTransformForBoundingBox(), self_and_contents_rect_out));
        return;
      }
      // Create |ScopedInlineItem| here because the decoration box is not
      // supported for SVG.
      InlinePaintContext::ScopedInlineItem scoped_inline_item(*this,
                                                              inline_context);
      ink_overflow_type_ =
          static_cast<unsigned>(ink_overflow_.SetTextInkOverflow(
              InkOverflowType(), cursor, paint_info, Style(),
              RectInContainerFragment(), inline_context,
              self_and_contents_rect_out));
      return;
    }

    ink_overflow_type_ =
        static_cast<unsigned>(ink_overflow_.Reset(InkOverflowType()));
    *self_and_contents_rect_out = LocalRect();
    return;
  }

  if (Type() == kBox) {
    const PhysicalBoxFragment* box_fragment = PostLayoutBoxFragment();
    if (UNLIKELY(!box_fragment))
      return;
    if (!box_fragment->IsInlineBox()) {
      DCHECK(!HasChildren());
      if (box_fragment->CanUseFragmentsForInkOverflow()) {
        box_fragment->GetMutableForPainting().RecalcInkOverflow();
        *self_and_contents_rect_out = box_fragment->InkOverflowRect();
        return;
      }
      LayoutBox* owner_box = MutableInkOverflowOwnerBox();
      DCHECK(owner_box);
      owner_box->RecalcNormalFlowChildVisualOverflowIfNeeded();
      *self_and_contents_rect_out = owner_box->VisualOverflowRect();
      return;
    }

    DCHECK(box_fragment->IsInlineBox());
    InlinePaintContext::ScopedInlineItem scoped_inline_item(*this,
                                                            inline_context);
    const PhysicalRect contents_rect =
        RecalcInkOverflowForDescendantsOf(cursor, inline_context);
    DCHECK(box_fragment->Children().empty());
    DCHECK_EQ(box_fragment->Size(), Size());
    box_fragment->GetMutableForPainting().RecalcInkOverflow(contents_rect);
    *self_and_contents_rect_out = box_fragment->InkOverflowRect();
    return;
  }

  if (Type() == kLine) {
    InlinePaintContext::ScopedLineBox scoped_line_box(cursor, inline_context);
    PhysicalRect contents_rect =
        RecalcInkOverflowForDescendantsOf(cursor, inline_context);
    const auto* const text_combine =
        DynamicTo<LayoutTextCombine>(GetLayoutObject());
    if (UNLIKELY(text_combine))
      contents_rect = text_combine->AdjustRectForBoundingBox(contents_rect);
    // Line boxes don't have self overflow. Compute content overflow only.
    *self_and_contents_rect_out = UnionRect(LocalRect(), contents_rect);
    ink_overflow_type_ = static_cast<unsigned>(
        ink_overflow_.SetContents(InkOverflowType(), contents_rect, Size()));
    return;
  }

  NOTREACHED();
}

PhysicalRect FragmentItem::RecalcInkOverflowForDescendantsOf(
    const InlineCursor& cursor,
    InlinePaintContext* inline_context) const {
  // Re-compute descendants, then compute the contents ink overflow from them.
  InlineCursor descendants_cursor = cursor.CursorForDescendants();
  PhysicalRect contents_rect =
      RecalcInkOverflowForCursor(&descendants_cursor, inline_context);

  // |contents_rect| is relative to the inline formatting context. Make it
  // relative to |this|.
  contents_rect.offset -= OffsetInContainerFragment();
  return contents_rect;
}

void FragmentItem::SetDeltaToNextForSameLayoutObject(wtf_size_t delta) const {
  DCHECK_NE(Type(), kLine);
  delta_to_next_for_same_layout_object_ = delta;
}

// Compute the inline position from text offset, in logical coordinate relative
// to this fragment.
LayoutUnit FragmentItem::InlinePositionForOffset(
    StringView text,
    unsigned offset,
    LayoutUnit (*round_function)(float),
    AdjustMidCluster adjust_mid_cluster) const {
  DCHECK_GE(offset, StartOffset());
  DCHECK_LE(offset, EndOffset());
  DCHECK_EQ(text.length(), TextLength());

  offset -= StartOffset();
  if (TextShapeResult()) {
    // TODO(layout-dev): Move caret position out of ShapeResult and into a
    // separate support class that can take a ShapeResult or ShapeResultView.
    // Allows for better code separation and avoids the extra copy below.
    return round_function(
        TextShapeResult()->CreateShapeResult()->CaretPositionForOffset(
            offset, text, adjust_mid_cluster));
  }

  // This fragment is a flow control because otherwise ShapeResult exists.
  DCHECK(IsFlowControl());
  DCHECK_EQ(1u, text.length());
  if (!offset || UNLIKELY(IsRtl(Style().Direction())))
    return LayoutUnit();
  if (Type() == kSvgText) {
    return LayoutUnit(IsHorizontal() ? GetSvgFragmentData()->rect.width()
                                     : GetSvgFragmentData()->rect.height());
  }
  return IsHorizontal() ? Size().width : Size().height;
}

LayoutUnit FragmentItem::CaretInlinePositionForOffset(StringView text,
                                                      unsigned offset) const {
  return InlinePositionForOffset(text, offset, LayoutUnit::FromFloatRound,
                                 AdjustMidCluster::kToEnd);
}

std::pair<LayoutUnit, LayoutUnit> FragmentItem::LineLeftAndRightForOffsets(
    StringView text,
    unsigned start_offset,
    unsigned end_offset) const {
  DCHECK_LE(start_offset, EndOffset());
  DCHECK_GE(start_offset, StartOffset());
  DCHECK_LE(end_offset, EndOffset());

  const LayoutUnit start_position =
      InlinePositionForOffset(text, start_offset, LayoutUnit::FromFloatFloor,
                              AdjustMidCluster::kToStart);
  const LayoutUnit end_position = InlinePositionForOffset(
      text, end_offset, LayoutUnit::FromFloatCeil, AdjustMidCluster::kToEnd);

  // Swap positions if RTL.
  return (UNLIKELY(start_position > end_position))
             ? std::make_pair(end_position, start_position)
             : std::make_pair(start_position, end_position);
}

PhysicalRect FragmentItem::LocalRect(StringView text,
                                     unsigned start_offset,
                                     unsigned end_offset) const {
  LayoutUnit width = Size().width;
  LayoutUnit height = Size().height;
  if (Type() == kSvgText) {
    const SvgFragmentData& data = *GetSvgFragmentData();
    if (IsHorizontal()) {
      width = LayoutUnit(data.rect.size().width() / data.length_adjust_scale);
      height = LayoutUnit(data.rect.size().height());
    } else {
      width = LayoutUnit(data.rect.size().width());
      height = LayoutUnit(data.rect.size().height() / data.length_adjust_scale);
    }
  }
  if (start_offset == StartOffset() && end_offset == EndOffset()) {
    return {LayoutUnit(), LayoutUnit(), width, height};
  }
  LayoutUnit start_position, end_position;
  std::tie(start_position, end_position) =
      LineLeftAndRightForOffsets(text, start_offset, end_offset);
  const LayoutUnit inline_size = end_position - start_position;
  switch (GetWritingMode()) {
    case WritingMode::kHorizontalTb:
      return {start_position, LayoutUnit(), inline_size, height};
    case WritingMode::kVerticalRl:
    case WritingMode::kVerticalLr:
    case WritingMode::kSidewaysRl:
      return {LayoutUnit(), start_position, width, inline_size};
    case WritingMode::kSidewaysLr:
      return {LayoutUnit(), height - end_position, width, inline_size};
  }
  NOTREACHED();
  return {};
}

PhysicalRect FragmentItem::ComputeTextBoundsRectForHitTest(
    const PhysicalOffset& inline_root_offset,
    bool is_occlusion_test) const {
  DCHECK(IsText());
  const PhysicalOffset offset =
      inline_root_offset + OffsetInContainerFragment();
  const PhysicalRect border_rect(offset, Size());
  if (UNLIKELY(is_occlusion_test)) {
    PhysicalRect ink_overflow = SelfInkOverflowRect();
    ink_overflow.Move(border_rect.offset);
    return ink_overflow;
  }
  // We should not ignore fractional parts of border_rect in SVG because this
  // item might have much larger screen size than border_rect.
  // See svg/hittest/text-small-font-size.html.
  if (Type() == kSvgText)
    return border_rect;
  return PhysicalRect(ToPixelSnappedRect(border_rect));
}

PositionWithAffinity FragmentItem::PositionForPointInText(
    const PhysicalOffset& point,
    const InlineCursor& cursor) const {
  DCHECK(Type() == kText || Type() == kSvgText);
  DCHECK_EQ(cursor.CurrentItem(), this);
  if (IsGeneratedText())
    return PositionWithAffinity();
  return PositionForPointInText(TextOffsetForPoint(point, cursor.Items()),
                                cursor);
}

PositionWithAffinity FragmentItem::PositionForPointInText(
    unsigned text_offset,
    const InlineCursor& cursor) const {
  DCHECK(Type() == kText || Type() == kSvgText);
  DCHECK_EQ(cursor.CurrentItem(), this);
  DCHECK(!IsGeneratedText());
  DCHECK_LE(text_offset, EndOffset());
  const CaretPosition unadjusted_position{
      cursor, CaretPositionType::kAtTextOffset, text_offset};
  if (RuntimeEnabledFeatures::BidiCaretAffinityEnabled())
    return unadjusted_position.ToPositionInDOMTreeWithAffinity();
  if (text_offset > StartOffset() && text_offset < EndOffset())
    return unadjusted_position.ToPositionInDOMTreeWithAffinity();
  return BidiAdjustment::AdjustForHitTest(unadjusted_position)
      .ToPositionInDOMTreeWithAffinity();
}

unsigned FragmentItem::TextOffsetForPoint(const PhysicalOffset& point,
                                          const FragmentItems& items) const {
  DCHECK(Type() == kText || Type() == kSvgText);
  const ComputedStyle& style = Style();
  const LayoutUnit& point_in_line_direction =
      style.IsHorizontalWritingMode() ? point.left : point.top;
  if (const ShapeResultView* shape_result = TextShapeResult()) {
    float scaled_offset = ScaleInlineOffset(point_in_line_direction);
    // TODO(layout-dev): Move caret logic out of ShapeResult into separate
    // support class for code health and to avoid this copy.
    return shape_result->CreateShapeResult()->CaretOffsetForHitTest(
               scaled_offset, Text(items), BreakGlyphsOption(true)) +
           StartOffset();
  }

  // Flow control fragments such as forced line break, tabulation, soft-wrap
  // opportunities, etc. do not have ShapeResult.
  DCHECK(IsFlowControl());

  // Zero-inline-size objects such as newline always return the start offset.
  LogicalSize size = Size().ConvertToLogical(style.GetWritingMode());
  if (!size.inline_size)
    return StartOffset();

  // Sized objects such as tabulation returns the next offset if the given point
  // is on the right half.
  LayoutUnit inline_offset = IsLtr(ResolvedDirection())
                                 ? point_in_line_direction
                                 : size.inline_size - point_in_line_direction;
  DCHECK_EQ(1u, TextLength());
  return inline_offset <= size.inline_size / 2 ? StartOffset() : EndOffset();
}

void FragmentItem::Trace(Visitor* visitor) const {
  visitor->Trace(layout_object_);
  // Looking up |const_trace_type_| inside Trace() is safe since it is const.
  if (const_traced_type_ == kLineItem)
    visitor->Trace(line_);
  else if (const_traced_type_ == kBoxItem)
    visitor->Trace(box_);
}

std::ostream& operator<<(std::ostream& ostream, const FragmentItem& item) {
  ostream << "{";
  switch (item.Type()) {
    case FragmentItem::kInvalid:
      NOTREACHED_NORETURN() << "Invalid FragmentItem";
    case FragmentItem::kText:
      ostream << "Text " << item.StartOffset() << "-" << item.EndOffset() << " "
              << (IsLtr(item.ResolvedDirection()) ? "LTR" : "RTL");
      break;
    case FragmentItem::kSvgText:
      ostream << "SvgText " << item.StartOffset() << "-" << item.EndOffset()
              << " " << (IsLtr(item.ResolvedDirection()) ? "LTR" : "RTL");
      break;
    case FragmentItem::kGeneratedText:
      ostream << "GeneratedText \"" << item.GeneratedText() << "\"";
      break;
    case FragmentItem::kLine:
      ostream << "Line #descendants=" << item.DescendantsCount() << " "
              << (IsLtr(item.BaseDirection()) ? "LTR" : "RTL");
      break;
    case FragmentItem::kBox:
      ostream << "Box #descendants=" << item.DescendantsCount();
      if (item.IsAtomicInline()) {
        ostream << " AtomicInline"
                << (IsLtr(item.ResolvedDirection()) ? "LTR" : "RTL");
      }
      break;
  }
  ostream << " ";
  switch (item.GetStyleVariant()) {
    case StyleVariant::kStandard:
      ostream << "Standard";
      break;
    case StyleVariant::kFirstLine:
      ostream << "FirstLine";
      break;
    case StyleVariant::kEllipsis:
      ostream << "Ellipsis";
      break;
  }
  return ostream << "}";
}

std::ostream& operator<<(std::ostream& ostream, const FragmentItem* item) {
  if (!item)
    return ostream << "<null>";
  return ostream << *item;
}

}  // namespace blink
