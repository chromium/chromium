// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/mojo_jni_bridge.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/bindings/lib/message_fragment.h"
#include "mojo/public/interfaces/bindings/tests/jni.test-mojom-jni.h"
#include "mojo/public/interfaces/bindings/tests/jni.test-mojom.h"
#include "mojo/public/interfaces/bindings/tests/rect.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must come after jni.test-mojom.h.
#include "mojo/public/interfaces/bindings/tests/jni.test-mojom-params-data.h"

namespace mojo::test {
namespace {

class TestJniResultStub : public jni::mojom::JniResultIfaceDirectReturnStub {
 public:
  TestJniResultStub() = default;
  ~TestJniResultStub() override = default;

  AddResult Add(int32_t a, int32_t b) override {
    if (a < 0) {
      return base::unexpected(
          jni::mojom::JniTestError::New(400, "negative input"));
    }
    return base::ok(a + b);
  }

  EchoResult Echo(const std::string& input) override {
    return base::ok("echo: " + input);
  }

  PingResult Ping() override { return base::ok(true); }
};

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

TEST(MojoJniBridgeTest, GeneratedDirectReturnStubDispatch) {
  JNIEnv* env = base::android::AttachCurrentThread();
  TestJniResultStub stub;
  MojoJniBridge<TestJniResultStub> bridge(&stub);

  Message request(/*name=*/0, Message::kFlagExpectsResponse, 0, 0, nullptr);
  ::mojo::internal::MessageFragment<
      jni::mojom::internal::JniResultIface_Add_Params_Data>
      params(request);
  params.Allocate();
  params->a = 15;
  params->b = 27;

  base::android::ScopedJavaLocalRef<jbyteArray> j_response =
      bridge.Invoke(env, std::move(request));
  ASSERT_TRUE(j_response);

  std::vector<uint8_t> response_bytes;
  base::android::JavaByteArrayToByteVector(env, j_response, &response_bytes);
  Message response_message(response_bytes, {});

  jni::mojom::JniResultIface_Add_ResponseParam_ResultPtr result;
  ASSERT_TRUE(jni::mojom::JniResultIface_Add_ResponseParam_Result::
                  DeserializeFromMessage(std::move(response_message), &result));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_success());
  EXPECT_EQ(42, result->get_success());
}

TEST(MojoJniBridgeTest, JniSerializeRejectsNullPtr) {
  JNIEnv* env = base::android::AttachCurrentThread();
  RectPtr null_rect;
  EXPECT_DEATH_IF_SUPPORTED(MojoDirectJniSerialize(env, null_rect), "");
}

}  // namespace
}  // namespace mojo::test
