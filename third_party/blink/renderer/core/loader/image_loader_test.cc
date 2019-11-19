// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/image_loader.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/html_image_loader.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class ImageLoaderTest : public RenderingTest {};

TEST_F(ImageLoaderTest, ReferrerPolicyChangeCausesUpdateOnInsert) {
  SetHtmlInnerHTML(R"HTML(
    <img id="test" src="test.png">
  )HTML");

  auto* element = GetDocument().getElementById("test");
  ASSERT_TRUE(element);

  auto* loader = MakeGarbageCollected<HTMLImageLoader>(element);
  ASSERT_TRUE(loader);

  // We should already be collected, so UpdateFromElement() would cause some
  // pending activity.
  loader->UpdateFromElement();
  ASSERT_TRUE(loader->HasPendingActivity());

  // We don't need an update, since we're already loading an image.
  EXPECT_FALSE(loader->ShouldUpdateOnInsertedInto(*element));

  // However, if the referrer policy changes, then we should need an update.
  EXPECT_TRUE(loader->ShouldUpdateOnInsertedInto(
      *element, network::mojom::ReferrerPolicy::kNever));

  // Changing referrer policy.
  loader->UpdateFromElement(ImageLoader::kUpdateNormal,
                            network::mojom::ReferrerPolicy::kNever);

  // Now, we don't need an update with the latest referrer policy.
  EXPECT_FALSE(loader->ShouldUpdateOnInsertedInto(
      *element, network::mojom::ReferrerPolicy::kNever));

  // But we do want an update if the referrer policy changes back to what it was
  // before.
  EXPECT_TRUE(loader->ShouldUpdateOnInsertedInto(
      *element, network::mojom::ReferrerPolicy::kDefault));
}

}  // namespace blink
