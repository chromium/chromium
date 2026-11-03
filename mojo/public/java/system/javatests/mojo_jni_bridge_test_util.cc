// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/bindings/mojo_jni_bridge.h"
#include "mojo/public/interfaces/bindings/tests/jni.test-mojom-jni.h"
#include "mojo/public/interfaces/bindings/tests/jni.test-mojom.h"
#include "mojo/public/interfaces/bindings/tests/rect.test-mojom.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "mojo/public/java/system/mojo_javatests_jni/MojoJniBridgeTestUtil_jni.h"

namespace mojo {

namespace android {

class TestJniStub {
 public:
  TestJniStub() = default;
  ~TestJniStub() = default;

  test::RectPtr TransposeRect(int32_t x,
                              int32_t y,
                              int32_t width,
                              int32_t height) {
    return test::Rect::New(y, x, height, width);
  }

  void RecordOneWayRect(int32_t x) { last_one_way_x_ = x; }

  int32_t last_one_way_x() const { return last_one_way_x_; }

 private:
  int32_t last_one_way_x_ = 0;
};

class TestJniResultStub
    : public test::jni::mojom::JniResultIfaceDirectReturnStub {
 public:
  TestJniResultStub() = default;
  ~TestJniResultStub() override = default;

  AddResult Add(int32_t a, int32_t b) override {
    if (a < 0) {
      return base::unexpected(
          test::jni::mojom::JniTestError::New(400, "negative input"));
    }
    return base::ok(a + b);
  }

  EchoResult Echo(const std::string& input) override {
    if (input.empty()) {
      return base::unexpected(
          test::jni::mojom::JniTestError::New(422, "empty input"));
    }
    return base::ok("echo: " + input);
  }

  PingResult Ping() override { return base::ok(true); }
};

}  // namespace android

template <>
struct MojoJniBridgeTraits<android::TestJniStub> {
  static base::android::ScopedJavaLocalRef<jbyteArray>
  Dispatch(JNIEnv* env, android::TestJniStub* stub, Message message) {
    CHECK(stub);
    const uint32_t method = message.name();
    test::RectPtr input;
    CHECK(test::Rect::DeserializeFromMessage(std::move(message), &input));
    switch (method) {
      case 0: {
        test::RectPtr result = stub->TransposeRect(input->x, input->y,
                                                   input->width, input->height);
        return MojoDirectJniSerialize(env, result);
      }
      case 1: {
        stub->RecordOneWayRect(input->x);
        return nullptr;
      }
    }
    NOTREACHED() << "Unexpected method ordinal: " << method;
  }
};

namespace android {

namespace {

using TestBridge = MojoJniBridge<TestJniStub>;
using TestResultBridge = MojoJniBridge<TestJniResultStub>;

TestJniStub* ToStub(int64_t stub) {
  CHECK(stub);
  return reinterpret_cast<TestJniStub*>(stub);
}

TestJniResultStub* ToResultStub(int64_t stub) {
  CHECK(stub);
  return reinterpret_cast<TestJniResultStub*>(stub);
}

}  // namespace

static int64_t JNI_MojoJniBridgeTestUtil_CreateStub(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(new TestJniStub());
}

static void JNI_MojoJniBridgeTestUtil_DestroyStub(JNIEnv* env, int64_t stub) {
  delete ToStub(stub);
}

static int64_t JNI_MojoJniBridgeTestUtil_CreateBridge(JNIEnv* env,
                                                      int64_t stub) {
  auto* bridge = new TestBridge(ToStub(stub));
  return reinterpret_cast<intptr_t>(
      static_cast<internal::MojoJniBridgeBase*>(bridge));
}

static void JNI_MojoJniBridgeTestUtil_DestroyBridge(JNIEnv* env,
                                                    int64_t bridge) {
  delete reinterpret_cast<internal::MojoJniBridgeBase*>(bridge);
}

static int32_t JNI_MojoJniBridgeTestUtil_GetLastOneWayX(JNIEnv* env,
                                                        int64_t stub) {
  return ToStub(stub)->last_one_way_x();
}

static int64_t JNI_MojoJniBridgeTestUtil_CreateResultStub(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(new TestJniResultStub());
}

static void JNI_MojoJniBridgeTestUtil_DestroyResultStub(JNIEnv* env,
                                                        int64_t stub) {
  delete ToResultStub(stub);
}

static int64_t JNI_MojoJniBridgeTestUtil_CreateResultBridge(JNIEnv* env,
                                                            int64_t stub) {
  auto* bridge = new TestResultBridge(ToResultStub(stub));
  return reinterpret_cast<intptr_t>(
      static_cast<internal::MojoJniBridgeBase*>(bridge));
}

}  // namespace android
}  // namespace mojo

DEFINE_JNI(MojoJniBridgeTestUtil)
