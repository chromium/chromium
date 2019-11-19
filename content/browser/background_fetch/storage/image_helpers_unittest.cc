// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/image_helpers.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {
namespace background_fetch {

namespace {

void DidSerializeIcon(base::OnceClosure quit_closure,
                      std::string* out_icon,
                      std::string icon) {
  DCHECK(out_icon);
  *out_icon = std::move(icon);
  std::move(quit_closure).Run();
}

void DidDeserializeIcon(base::OnceClosure quit_closure,
                        SkBitmap* out_icon,
                        SkBitmap icon) {
  DCHECK(out_icon);
  *out_icon = std::move(icon);
  std::move(quit_closure).Run();
}

TEST(BackgroundFetchImageHelpers, ShouldPersistIcon) {
  SkBitmap null_icon;
  EXPECT_FALSE(ShouldPersistIcon(null_icon));

  SkBitmap large_icon;
  large_icon.allocN32Pixels(512, 512);
  EXPECT_FALSE(ShouldPersistIcon(large_icon));

  SkBitmap valid_icon;
  valid_icon.allocN32Pixels(42, 42);
  EXPECT_TRUE(ShouldPersistIcon(valid_icon));
}

TEST(BackgroundFetchImageHelpers, SerializeRoundTrip) {
  base::test::TaskEnvironment task_environment;

  SkBitmap icon;
  icon.allocN32Pixels(42, 42);
  icon.eraseColor(SK_ColorGREEN);

  // Serialize.
  std::string serialized_icon;
  {
    base::RunLoop run_loop;
    SerializeIcon(icon,
                  base::BindOnce(&DidSerializeIcon, run_loop.QuitClosure(),
                                 &serialized_icon));
    run_loop.Run();
  }

  // Deserialize.
  SkBitmap result_icon;
  {
    base::RunLoop run_loop;
    DeserializeIcon(std::make_unique<std::string>(serialized_icon),
                    base::BindOnce(&DidDeserializeIcon, run_loop.QuitClosure(),
                                   &result_icon));
    run_loop.Run();
  }

  ASSERT_FALSE(result_icon.isNull());
  EXPECT_EQ(icon.height(), result_icon.height());
  EXPECT_EQ(icon.width(), result_icon.width());
  for (int i = 0; i < result_icon.width(); i++) {
    for (int j = 0; j < result_icon.height(); j++)
      EXPECT_EQ(result_icon.getColor(i, j), SK_ColorGREEN);
  }
}

}  // namespace

}  // namespace background_fetch
}  // namespace content
