// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/ranges/algorithm.h"
#include "mojo/public/c/system/core.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/platform_handle.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "mojo/public/java/system/system_impl_java_jni_headers/CoreImpl_jni.h"

namespace mojo {
namespace android {

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

static jlong JNI_CoreImpl_GetTimeTicksNow(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  return MojoGetTimeTicksNow();
}

static ScopedJavaLocalRef<jobject> JNI_CoreImpl_CreateMessagePipe(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jobject>& options_buffer) {
  const MojoCreateMessagePipeOptions* options = NULL;
  if (options_buffer) {
    const void* buffer_start = env->GetDirectBufferAddress(options_buffer);
    DCHECK(buffer_start);
    DCHECK_EQ(reinterpret_cast<const uintptr_t>(buffer_start) % 8, 0u);
    const size_t buffer_size = env->GetDirectBufferCapacity(options_buffer);
    DCHECK_EQ(buffer_size, sizeof(MojoCreateMessagePipeOptions));
    options = static_cast<const MojoCreateMessagePipeOptions*>(buffer_start);
    DCHECK_EQ(options->struct_size, buffer_size);
  }
  MojoHandle handle1;
  MojoHandle handle2;
  MojoResult result = MojoCreateMessagePipe(options, &handle1, &handle2);
  return Java_CoreImpl_newNativeCreationResult(env, result, handle1, handle2);
}

static ScopedJavaLocalRef<jobject> JNI_CoreImpl_CreateDataPipe(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jobject>& options_buffer) {
  const MojoCreateDataPipeOptions* options = NULL;
  if (options_buffer) {
    const void* buffer_start = env->GetDirectBufferAddress(options_buffer);
    DCHECK(buffer_start);
    DCHECK_EQ(reinterpret_cast<const uintptr_t>(buffer_start) % 8, 0u);
    const size_t buffer_size = env->GetDirectBufferCapacity(options_buffer);
    DCHECK_EQ(buffer_size, sizeof(MojoCreateDataPipeOptions));
    options = static_cast<const MojoCreateDataPipeOptions*>(buffer_start);
    DCHECK_EQ(options->struct_size, buffer_size);
  }
  MojoHandle handle1;
  MojoHandle handle2;
  MojoResult result = MojoCreateDataPipe(options, &handle1, &handle2);
  return Java_CoreImpl_newNativeCreationResult(env, result, handle1, handle2);
}

static ScopedJavaLocalRef<jobject> JNI_CoreImpl_CreateSharedBuffer(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jobject>& options_buffer,
    jlong num_bytes) {
  const MojoCreateSharedBufferOptions* options = 0;
  if (options_buffer) {
    const void* buffer_start = env->GetDirectBufferAddress(options_buffer);
    DCHECK(buffer_start);
    DCHECK_EQ(reinterpret_cast<const uintptr_t>(buffer_start) % 8, 0u);
    const size_t buffer_size = env->GetDirectBufferCapacity(options_buffer);
    DCHECK_EQ(buffer_size, sizeof(MojoCreateSharedBufferOptions));
    options = static_cast<const MojoCreateSharedBufferOptions*>(buffer_start);
    DCHECK_EQ(options->struct_size, buffer_size);
  }
  MojoHandle handle;
  MojoResult result = MojoCreateSharedBuffer(num_bytes, options, &handle);
  return Java_CoreImpl_newResultAndLong(env, result, handle);
}

static jint JNI_CoreImpl_Close(JNIEnv* env,
                               const JavaParamRef<jobject>& jcaller,
                               jlong mojo_handle) {
  return MojoClose(mojo_handle);
}

static jint JNI_CoreImpl_QueryHandleSignalsState(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jlong mojo_handle,
    const JavaParamRef<jobject>& buffer) {
  MojoHandleSignalsState* signals_state =
      static_cast<MojoHandleSignalsState*>(env->GetDirectBufferAddress(buffer));
  DCHECK(signals_state);
  DCHECK_EQ(sizeof(MojoHandleSignalsState),
            static_cast<size_t>(env->GetDirectBufferCapacity(buffer)));
  return MojoQueryHandleSignalsState(mojo_handle, signals_state);
}

static jint JNI_CoreImpl_WriteMessage(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jlong mojo_handle,
    const JavaParamRef<jobject>& bytes,
    jint num_bytes,
    const JavaParamRef<jobject>& handles_buffer,
    jint flags) {
  const void* buffer_start = 0;
  uint32_t buffer_size = 0;
  if (bytes) {
    buffer_start = env->GetDirectBufferAddress(bytes);
    DCHECK(buffer_start);
    DCHECK(env->GetDirectBufferCapacity(bytes) >= num_bytes);
    buffer_size = num_bytes;
  }
  const jlong* java_handles = nullptr;
  uint32_t num_handles = 0;
  if (handles_buffer) {
    java_handles =
        static_cast<jlong*>(env->GetDirectBufferAddress(handles_buffer));
    num_handles = env->GetDirectBufferCapacity(handles_buffer) / sizeof(jlong);
  }

  // Truncate handle values if necessary.
  std::vector<MojoHandle> handles(num_handles);
  std::copy(java_handles, java_handles + num_handles, handles.begin());

  // Java code will handle invalidating handles if the write succeeded.
  return WriteMessageRaw(
      MessagePipeHandle(static_cast<MojoHandle>(mojo_handle)), buffer_start,
      buffer_size, handles.data(), num_handles, flags);
}

static ScopedJavaLocalRef<jobject> JNI_CoreImpl_ReadMessage(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jlong mojo_handle,
    jint flags) {
  ScopedMessageHandle message;
  MojoResult result =
      ReadMessageNew(MessagePipeHandle(mojo_handle), &message, flags);
  if (result != MOJO_RESULT_OK)
    return Java_CoreImpl_newReadMessageResult(env, result, nullptr, nullptr);
  DCHECK(message.is_valid());

  result = MojoSerializeMessage(message->value(), nullptr);
  if (result != MOJO_RESULT_OK && result != MOJO_RESULT_FAILED_PRECONDITION) {
    return Java_CoreImpl_newReadMessageResult(env, MOJO_RESULT_ABORTED, nullptr,
                                              nullptr);
  }

  uint32_t num_bytes;
  void* buffer;
  uint32_t num_handles = 0;
  std::vector<MojoHandle> handles;
  result = MojoGetMessageData(message->value(), nullptr, &buffer, &num_bytes,
                              nullptr, &num_handles);
  if (result == MOJO_RESULT_RESOURCE_EXHAUSTED) {
    handles.resize(num_handles);
    result = MojoGetMessageData(message->value(), nullptr, &buffer, &num_bytes,
                                handles.data(), &num_handles);
  }

  if (result != MOJO_RESULT_OK)
    return Java_CoreImpl_newReadMessageResult(env, result, nullptr, nullptr);

  // Extend handles to 64-bit values if necessary.
  std::vector<int64_t> java_handles(handles.size());
  base::ranges::copy(handles, java_handles.begin());
  return Java_CoreImpl_newReadMessageResult(
      env, result,
      base::android::ToJavaByteArray(env, static_cast<uint8_t*>(buffer),
                                     num_bytes),
      base::android::ToJavaLongArray(env, java_handles));
}

static ScopedJavaLocalRef<jobject> JNI_CoreImpl_ReadData(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jlong mojo_handle,
    const JavaParamRef<jobject>& elements,
    jint elements_capacity,
    jint flags) {
  void* buffer_start = 0;
  uint32_t buffer_size = elements_capacity;
  if (elements) {
    buffer_start = env->GetDirectBufferAddress(elements);
    DCHECK(buffer_start);
    DCHECK(elements_capacity <= env->GetDirectBufferCapacity(elements));
  }
  MojoReadDataOptions options;
  options.struct_size = sizeof(options);
  options.flags = flags;
  MojoResult result =
      MojoReadData(mojo_handle, &options, buffer_start, &buffer_size);
  return Java_CoreImpl_newResultAndInteger(
      env, result, (result == MOJO_RESULT_OK) ? buffer_size : 0);
}

static ScopedJavaLocalRef<jobject> JNI_CoreImpl_BeginReadData(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jlong mojo_handle,
    jint num_bytes,
    jint flags) {
  void const* buffer = 0;
  uint32_t buffer_size = num_bytes;

  MojoBeginReadDataOptions options;
  options.struct_size = sizeof(options);
  options.flags = flags;
  MojoResult result =
      MojoBeginReadData(mojo_handle, &options, &buffer, &buffer_size);
  if (result == MOJO_RESULT_OK) {
    ScopedJavaLocalRef<jobject> byte_buffer(
        env, env->NewDirectByteBuffer(const_cast<void*>(buffer), buffer_size));
    base::android::CheckException(env);
    return Java_CoreImpl_newResultAndBuffer(env, result, byte_buffer);
  } else {
    return Java_CoreImpl_newResultAndBuffer(env, result, nullptr);
  }
}

static jint JNI_CoreImpl_EndReadData(JNIEnv* env,
                                     const JavaParamRef<jobject>& jcaller,
                                     jlong mojo_handle,
                                     jint num_bytes_read) {
  return MojoEndReadData(mojo_handle, num_bytes_read, nullptr);
}

static ScopedJavaLocalRef<jobject> JNI_CoreImpl_WriteData(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jlong mojo_handle,
    const JavaParamRef<jobject>& elements,
    jint limit,
    jint flags) {
  void* buffer_start = env->GetDirectBufferAddress(elements);
  DCHECK(buffer_start);
  DCHECK(limit <= env->GetDirectBufferCapacity(elements));
  uint32_t buffer_size = limit;

  MojoWriteDataOptions options;
  options.struct_size = sizeof(options);
  options.flags = flags;
  MojoResult result =
      MojoWriteData(mojo_handle, buffer_start, &buffer_size, &options);
  return Java_CoreImpl_newResultAndInteger(
      env, result, (result == MOJO_RESULT_OK) ? buffer_size : 0);
}

static ScopedJavaLocalRef<jobject> JNI_CoreImpl_BeginWriteData(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jlong mojo_handle,
    jint num_bytes,
    jint flags) {
  void* buffer = 0;
  uint32_t buffer_size = num_bytes;
  MojoBeginWriteDataOptions options;
  options.struct_size = sizeof(options);
  options.flags = flags;
  MojoResult result =
      MojoBeginWriteData(mojo_handle, &options, &buffer, &buffer_size);
  if (result == MOJO_RESULT_OK) {
    ScopedJavaLocalRef<jobject> byte_buffer(
        env, env->NewDirectByteBuffer(buffer, buffer_size));
    base::android::CheckException(env);
    return Java_CoreImpl_newResultAndBuffer(env, result, byte_buffer);
  } else {
    return Java_CoreImpl_newResultAndBuffer(env, result, nullptr);
  }
}

static jint JNI_CoreImpl_EndWriteData(JNIEnv* env,
                                      const JavaParamRef<jobject>& jcaller,
                                      jlong mojo_handle,
                                      jint num_bytes_written) {
  return MojoEndWriteData(mojo_handle, num_bytes_written, nullptr);
}

static ScopedJavaLocalRef<jobject> JNI_CoreImpl_Duplicate(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jlong mojo_handle,
    const JavaParamRef<jobject>& options_buffer) {
  const MojoDuplicateBufferHandleOptions* options = 0;
  if (options_buffer) {
    const void* buffer_start = env->GetDirectBufferAddress(options_buffer);
    DCHECK(buffer_start);
    const size_t buffer_size = env->GetDirectBufferCapacity(options_buffer);
    DCHECK_EQ(buffer_size, sizeof(MojoDuplicateBufferHandleOptions));
    options =
        static_cast<const MojoDuplicateBufferHandleOptions*>(buffer_start);
    DCHECK_EQ(options->struct_size, buffer_size);
  }
  MojoHandle handle;
  MojoResult result = MojoDuplicateBufferHandle(mojo_handle, options, &handle);
  return Java_CoreImpl_newResultAndLong(env, result, handle);
}

static ScopedJavaLocalRef<jobject> JNI_CoreImpl_Map(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jlong mojo_handle,
    jlong offset,
    jlong num_bytes,
    jint flags) {
  void* buffer = 0;
  MojoMapBufferOptions options;
  options.struct_size = sizeof(options);
  options.flags = flags;
  MojoResult result =
      MojoMapBuffer(mojo_handle, offset, num_bytes, &options, &buffer);
  if (result == MOJO_RESULT_OK) {
    ScopedJavaLocalRef<jobject> byte_buffer(
        env, env->NewDirectByteBuffer(buffer, num_bytes));
    base::android::CheckException(env);
    return Java_CoreImpl_newResultAndBuffer(env, result, byte_buffer);
  } else {
    return Java_CoreImpl_newResultAndBuffer(env, result, nullptr);
  }
}

static int JNI_CoreImpl_Unmap(JNIEnv* env,
                              const JavaParamRef<jobject>& jcaller,
                              const JavaParamRef<jobject>& buffer) {
  void* buffer_start = env->GetDirectBufferAddress(buffer);
  DCHECK(buffer_start);
  return MojoUnmapBuffer(buffer_start);
}

static jint JNI_CoreImpl_GetNativeBufferOffset(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jobject>& buffer,
    jint alignment) {
  jint offset =
      reinterpret_cast<uintptr_t>(env->GetDirectBufferAddress(buffer)) %
      alignment;
  if (offset == 0)
    return 0;
  return alignment - offset;
}

static jlong JNI_CoreImpl_CreatePlatformHandle(JNIEnv* env, jint fd) {
  mojo::ScopedHandle handle =
      mojo::WrapPlatformHandle(mojo::PlatformHandle(base::ScopedFD(fd)));
  return handle.release().value();
}

}  // namespace android
}  // namespace mojo
