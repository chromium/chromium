// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_style_image_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_url_data.h"
#include "third_party/blink/renderer/core/css/cssom/css_url_image_value.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

class FakeCSSStyleImageValue : public CSSStyleImageValue {
 public:
  FakeCSSStyleImageValue(bool cache_pending, gfx::Size size)
      : cache_pending_(cache_pending), size_(size) {}

  // CSSStyleImageValue
  std::optional<gfx::Size> IntrinsicSize() const final {
    if (cache_pending_) {
      return std::nullopt;
    }
    return size_;
  }

  // CanvasImageSource
  scoped_refptr<Image> GetSourceImageForCanvas(SourceImageStatus*,
                                               const gfx::SizeF&) final {
    return nullptr;
  }
  ResourceStatus Status() const final {
    if (cache_pending_) {
      return ResourceStatus::kNotStarted;
    }
    return ResourceStatus::kCached;
  }
  bool IsAccelerated() const final { return false; }

  // CSSStyleValue
  const CSSValue* ToCSSValue() const final { return nullptr; }
  StyleValueType GetType() const final { return kUnknownType; }

 private:
  bool cache_pending_;
  gfx::Size size_;
};

}  // namespace

TEST(CSSStyleImageValueTest, PendingCache) {
  FakeCSSStyleImageValue* style_image_value =
      MakeGarbageCollected<FakeCSSStyleImageValue>(true, gfx::Size(100, 100));
  bool is_null = false;
  EXPECT_EQ(style_image_value->intrinsicWidth(is_null), 0);
  EXPECT_EQ(style_image_value->intrinsicHeight(is_null), 0);
  EXPECT_EQ(style_image_value->intrinsicRatio(is_null), 0);
  EXPECT_TRUE(is_null);
}

TEST(CSSStyleImageValueTest, ValidLoadedImage) {
  FakeCSSStyleImageValue* style_image_value =
      MakeGarbageCollected<FakeCSSStyleImageValue>(false, gfx::Size(480, 120));
  bool is_null = false;
  EXPECT_EQ(style_image_value->intrinsicWidth(is_null), 480);
  EXPECT_EQ(style_image_value->intrinsicHeight(is_null), 120);
  EXPECT_EQ(style_image_value->intrinsicRatio(is_null), 4);
  EXPECT_FALSE(is_null);
}

TEST(CSSURLImageValueTest, GetSourceImageForCanvasSetsStatus) {
  auto* url_data = MakeGarbageCollected<CSSUrlData>(
      AtomicString("https://example.com/image.png"));
  auto* css_image_value = MakeGarbageCollected<CSSImageValue>(*url_data);
  auto* url_image_value =
      MakeGarbageCollected<CSSURLImageValue>(*css_image_value);

  SourceImageStatus status = kNormalSourceImageStatus;
  url_image_value->GetSourceImageForCanvas(&status, gfx::SizeF(100, 100));
  // Since the image is not loaded, GetImage() returns null, and status should
  // be kInvalidSourceImageStatus.
  EXPECT_EQ(status, kInvalidSourceImageStatus);
}

}  // namespace blink
