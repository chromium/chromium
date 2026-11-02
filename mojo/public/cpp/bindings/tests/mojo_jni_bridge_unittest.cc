// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/mojo_jni_bridge.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "mojo/public/interfaces/bindings/tests/rect.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo::test {
namespace {

TEST(MojoJniBridgeTest, SerializeToJavaThenBackToNative) {
  JNIEnv* env = base::android::AttachCurrentThread();
  RectPtr input = Rect::New(7, 8, 90, 100);

  base::android::ScopedJavaLocalRef<jbyteArray> j_bytes =
      MojoDirectJniSerialize(env, input);
  ASSERT_TRUE(j_bytes);

  std::vector<uint8_t> bytes;
  base::android::JavaByteArrayToByteVector(env, j_bytes, &bytes);

  Message message(bytes, {});
  RectPtr output;
  ASSERT_TRUE(Rect::DeserializeFromMessage(std::move(message), &output));
  ASSERT_TRUE(output);
  EXPECT_EQ(7, output->x);
  EXPECT_EQ(8, output->y);
  EXPECT_EQ(90, output->width);
  EXPECT_EQ(100, output->height);
}

TEST(MojoJniBridgeTest, JniSerializeRejectsNullPtr) {
  JNIEnv* env = base::android::AttachCurrentThread();
  RectPtr null_rect;
  EXPECT_DEATH_IF_SUPPORTED(MojoDirectJniSerialize(env, null_rect), "");
}

}  // namespace
}  // namespace mojo::test
