// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/ui/page_display_state.h"

#include <cmath>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {
// Serialiation keys.
NSString* const kContentOffsetKey = @"contentOffset";
NSString* const kContentInsetKey = @"contentInset";
NSString* const kMinZoomKey = @"minZoom";
NSString* const kMaxZoomKey = @"maxZoom";
NSString* const kZoomKey = @"zoom";
// Deprecated serialization keys.
// TODO(crbug.com/926041): Remove these keys.
NSString* const kDeprecatedXOffsetKey = @"scrollX";
NSString* const kDeprecatedYOffsetKey = @"scrollY";
// Invalid consts.
const CGPoint kInvalidContentOffset = {NAN, NAN};
const UIEdgeInsets kInvalidContentInset = {NAN, NAN, NAN, NAN};
// Equality checkers.  Return true if both values are NAN or equivalent.
inline bool StateValuesAreEqual(CGFloat value1, CGFloat value2) {
  return std::isnan(value1) ? std::isnan(value2) : value1 == value2;
}
inline bool StateContentOffsetsAreEqual(const CGPoint& offset1,
                                        const CGPoint& offset2) {
  return StateValuesAreEqual(offset1.x, offset2.x) &&
         StateValuesAreEqual(offset1.y, offset2.y);
}
inline bool StateContentInsetsAreEqual(const UIEdgeInsets& inset1,
                                       const UIEdgeInsets& inset2) {
  return StateValuesAreEqual(inset1.top, inset2.top) &&
         StateValuesAreEqual(inset1.left, inset2.left) &&
         StateValuesAreEqual(inset1.bottom, inset2.bottom) &&
         StateValuesAreEqual(inset1.right, inset2.right);
}
// Validity checker util functions.
inline bool IsContentOffsetValid(const CGPoint& content_offset) {
  return !std::isnan(content_offset.x) && !std::isnan(content_offset.y);
}
inline bool IsContentInsetValid(const UIEdgeInsets& content_inset) {
  return !std::isnan(content_inset.top) && !std::isnan(content_inset.left) &&
         !std::isnan(content_inset.bottom) && !std::isnan(content_inset.right);
}
// Returns the CGFloat stored under |key| in |serialization|, or NAN if it is
// not set.
inline CGFloat GetFloatValue(NSString* key, NSDictionary* serialization) {
  NSNumber* value = serialization[key];
  return value ? [value doubleValue] : NAN;
}
// Returns the contentOffset stored in |serialization|, or a NAN offset if it is
// not set.
inline CGPoint GetContentOffset(NSDictionary* serialization) {
  NSValue* value = serialization[kContentOffsetKey];
  if (value)
    return [value CGPointValue];
  // TODO(crbug.com/926041): Return kInvalidContentOffset when legacy keys are
  // removed.
  return CGPointMake(GetFloatValue(kDeprecatedXOffsetKey, serialization),
                     GetFloatValue(kDeprecatedYOffsetKey, serialization));
}
// Returns the contentInset stored in |serialization|, or a NAN inset if it is
// not set.
inline UIEdgeInsets GetContentInset(NSDictionary* serialization) {
  NSValue* value = serialization[kContentInsetKey];
  if (value)
    return [value UIEdgeInsetsValue];
  if (serialization[kDeprecatedXOffsetKey] &&
      serialization[kDeprecatedYOffsetKey]) {
    // When restoring PageScrollStates created using the deprecated
    // serialization keyes, use UIEdgeInsetsZero as default.
    // TODO(crbug.com/926041): Just return kInvalidContentInset when legacy keys
    // are removed.
    return UIEdgeInsetsZero;
  }
  // Return an invalid inset if neither the new nor legacy keys were contained.
  return kInvalidContentInset;
}
}  // namespace

PageScrollState::PageScrollState()
    : content_offset_(kInvalidContentOffset),
      content_inset_(kInvalidContentInset) {}

PageScrollState::PageScrollState(const CGPoint& content_offset,
                                 const UIEdgeInsets& content_inset)
    : content_offset_(content_offset), content_inset_(content_inset) {}

PageScrollState::~PageScrollState() = default;

bool PageScrollState::IsValid() const {
  return IsContentOffsetValid(content_offset_) &&
         IsContentInsetValid(content_inset_);
}

CGPoint PageScrollState::GetEffectiveContentOffsetForContentInset(
    UIEdgeInsets content_inset) const {
  return CGPointMake(
      content_offset_.x + content_inset_.left - content_inset.left,
      content_offset_.y + content_inset_.top - content_inset.top);
}

bool PageScrollState::operator==(const PageScrollState& other) const {
  return StateContentOffsetsAreEqual(content_offset_, other.content_offset_) &&
         StateContentInsetsAreEqual(content_inset_, other.content_inset_);
}

bool PageScrollState::operator!=(const PageScrollState& other) const {
  return !(*this == other);
}

PageZoomState::PageZoomState()
    : minimum_zoom_scale_(NAN), maximum_zoom_scale_(NAN), zoom_scale_(NAN) {}

PageZoomState::PageZoomState(CGFloat minimum_zoom_scale,
                             CGFloat maximum_zoom_scale,
                             CGFloat zoom_scale)
    : minimum_zoom_scale_(minimum_zoom_scale),
      maximum_zoom_scale_(maximum_zoom_scale),
      zoom_scale_(zoom_scale) {}

PageZoomState::~PageZoomState() {}

bool PageZoomState::IsValid() const {
  return (!std::isnan(minimum_zoom_scale_) &&
          !std::isnan(maximum_zoom_scale_) && !std::isnan(zoom_scale_) &&
          zoom_scale_ >= minimum_zoom_scale_ &&
          zoom_scale_ <= maximum_zoom_scale_);
}

bool PageZoomState::operator==(const PageZoomState& other) const {
  return StateValuesAreEqual(minimum_zoom_scale_, other.minimum_zoom_scale_) &&
         StateValuesAreEqual(maximum_zoom_scale_, other.maximum_zoom_scale_) &&
         StateValuesAreEqual(zoom_scale_, other.zoom_scale_);
}

bool PageZoomState::operator!=(const PageZoomState& other) const {
  return !(*this == other);
}

PageDisplayState::PageDisplayState() {}

PageDisplayState::PageDisplayState(const PageScrollState& scroll_state,
                                   const PageZoomState& zoom_state)
    : scroll_state_(scroll_state), zoom_state_(zoom_state) {}

PageDisplayState::PageDisplayState(const CGPoint& content_offset,
                                   const UIEdgeInsets& content_inset,
                                   CGFloat minimum_zoom_scale,
                                   CGFloat maximum_zoom_scale,
                                   CGFloat zoom_scale)
    : scroll_state_(content_offset, content_inset),
      zoom_state_(minimum_zoom_scale, maximum_zoom_scale, zoom_scale) {}

PageDisplayState::PageDisplayState(NSDictionary* serialization)
    : PageDisplayState(GetContentOffset(serialization),
                       GetContentInset(serialization),
                       GetFloatValue(kMinZoomKey, serialization),
                       GetFloatValue(kMaxZoomKey, serialization),
                       GetFloatValue(kZoomKey, serialization)) {}

PageDisplayState::~PageDisplayState() {}

bool PageDisplayState::IsValid() const {
  return scroll_state_.IsValid() && zoom_state_.IsValid();
}

bool PageDisplayState::operator==(const PageDisplayState& other) const {
  return scroll_state_ == other.scroll_state_ &&
         zoom_state_ == other.zoom_state_;
}

bool PageDisplayState::operator!=(const PageDisplayState& other) const {
  return !(*this == other);
}

NSDictionary* PageDisplayState::GetSerialization() const {
  return @{
    kContentOffsetKey :
        [NSValue valueWithCGPoint:scroll_state_.content_offset()],
    kContentInsetKey :
        [NSValue valueWithUIEdgeInsets:scroll_state_.content_inset()],
    kMinZoomKey : @(zoom_state_.minimum_zoom_scale()),
    kMaxZoomKey : @(zoom_state_.maximum_zoom_scale()),
    kZoomKey : @(zoom_state_.zoom_scale())
  };
}

NSString* PageDisplayState::GetDescription() const {
  NSString* const kPageScrollStateDescriptionFormat =
      @"{ contentOffset:%@, contentInset:%@, zoomScaleRange:(%0.2f, %0.2f), "
      @"zoomScale:%0.2f }";
  return [NSString
      stringWithFormat:kPageScrollStateDescriptionFormat,
                       NSStringFromCGPoint(scroll_state_.content_offset()),
                       NSStringFromUIEdgeInsets(scroll_state_.content_inset()),
                       zoom_state_.minimum_zoom_scale(),
                       zoom_state_.maximum_zoom_scale(),
                       zoom_state_.zoom_scale()];
}

}  // namespace web
