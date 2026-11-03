// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MOJO_JNI_BRIDGE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MOJO_JNI_BRIDGE_H_

#include <jni.h>

#include <utility>

#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/bindings/message.h"

// Generic JNI transport for mojom(generate_jni = true) bindings.
//
// This can be thought as a separate transport layer for mojo messages in
// addition to the IPC transport.
//
// Route all JNI through a single class because:
//
//   1. Benchmarks showed that it was more optimal to serialize mojo messages
//      as a byte array then deserialize natively in Java (compared to using
//      JNI to construct messages).
//   2. Keeps the bloat from generated files low. JNI interop points cannot
//      be easily stripped by the compiler, so we want to make sure that we
//      have as few of them as possible.

namespace mojo {

namespace internal {

base::android::ScopedJavaLocalRef<jbyteArray> MessageToJavaByteArray(
    JNIEnv* env,
    const Message& message);

// Base class of every native object that Java reaches through
// MojoJniTransport. The `long bridge` held by Java is a pointer to this type,
// so subclasses must keep it as their first (primary) base.
class MojoJniBridgeBase {
 public:
  virtual ~MojoJniBridgeBase() = default;

  // Dispatches |message| (header + payload) and returns the serialized
  // response message, or null if there is none.
  virtual base::android::ScopedJavaLocalRef<jbyteArray> Invoke(
      JNIEnv* env,
      Message message) = 0;
};

}  // namespace internal

// Specialized by generated <mojom>.h headers for each DirectReturnStub
// interface. Provides:
//   static base::android::ScopedJavaLocalRef<jbyteArray> Dispatch(
//       JNIEnv*, <Interface>DirectReturnStub*, Message message);
template <typename T>
struct MojoJniBridgeTraits;

// Exposes a DirectReturnStub to Java.
template <typename StubType>
class MojoJniBridge : public internal::MojoJniBridgeBase {
 public:
  explicit MojoJniBridge(StubType* stub) : stub_(stub) { CHECK(stub); }

  MojoJniBridge(const MojoJniBridge&) = delete;
  MojoJniBridge& operator=(const MojoJniBridge&) = delete;

  ~MojoJniBridge() override = default;

  StubType* stub() const { return stub_; }

  // internal::MojoJniBridgeBase:
  base::android::ScopedJavaLocalRef<jbyteArray> Invoke(
      JNIEnv* env,
      Message message) override {
    return MojoJniBridgeTraits<StubType>::Dispatch(env, stub_,
                                                   std::move(message));
  }

 private:
  raw_ptr<StubType> stub_;
};

// Serializes a mojom struct or union as a Mojo message (header + payload) for
// Java.
template <typename PtrType>
base::android::ScopedJavaLocalRef<jbyteArray> MojoDirectJniSerialize(
    JNIEnv* env,
    const PtrType& value) {
  CHECK(value);
  using StructType = typename PtrType::Struct;
  Message message =
      StructType::SerializeAsMessage(const_cast<PtrType*>(&value));
  CHECK(message.payload_num_bytes());
  return internal::MessageToJavaByteArray(env, message);
}

// Serializes a direct-return result<T, E> as its generated response union
// message for Java.
template <typename UnionType, typename T, typename E>
base::android::ScopedJavaLocalRef<jbyteArray> MojoDirectJniSerialize(
    JNIEnv* env,
    base::expected<T, E> result) {
  auto response = result.has_value()
                      ? UnionType::NewSuccess(std::move(result.value()))
                      : UnionType::NewFailure(std::move(result.error()));
  Message message = UnionType::SerializeAsMessage(&response);
  return internal::MessageToJavaByteArray(env, message);
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MOJO_JNI_BRIDGE_H_
