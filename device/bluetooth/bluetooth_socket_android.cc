// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_socket_android.h"

#include <jni.h>

#include <cstdint>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notimplemented.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread.h"
#include "device/bluetooth/android/outcome.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "net/base/io_buffer.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "device/bluetooth/jni_headers/ChromeBluetoothSocket_jni.h"

namespace device {

namespace {

using ::base::android::AttachCurrentThread;

void DeactivateSocket(scoped_refptr<BluetoothSocketThread> socket_thread) {
  socket_thread->OnSocketDeactivate();
}

}  // namespace

scoped_refptr<BluetoothSocketAndroid> BluetoothSocketAndroid::Create(
    base::android::ScopedJavaLocalRef<jobject> socket_wrapper,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<BluetoothSocketThread> socket_thread) {
  return base::MakeRefCounted<BluetoothSocketAndroid>(std::move(socket_wrapper),
                                                      std::move(task_runner),
                                                      std::move(socket_thread));
}

BluetoothSocketAndroid::BluetoothSocketAndroid(
    base::android::ScopedJavaLocalRef<jobject> socket_wrapper,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<BluetoothSocketThread> socket_thread)
    : ui_task_runner_(ui_task_runner),
      socket_thread_(socket_thread),
      j_socket_(Java_ChromeBluetoothSocket_create(AttachCurrentThread(),
                                                  socket_wrapper)) {
  CHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  socket_thread_->OnSocketActivate();
}

BluetoothSocketAndroid::~BluetoothSocketAndroid() {
  CHECK(!Java_ChromeBluetoothSocket_isConnected(AttachCurrentThread(),
                                                j_socket_));
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DeactivateSocket, std::move(socket_thread_)));
}

base::android::ScopedJavaLocalRef<jobject>
BluetoothSocketAndroid::GetJavaObject() const {
  return base::android::ScopedJavaLocalRef<jobject>(AttachCurrentThread(),
                                                    j_socket_);
}

void BluetoothSocketAndroid::DispatchErrorCallback(
    ErrorCompletionCallback error_callback,
    const Outcome& outcome) {
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(error_callback), outcome.GetExceptionMessage()));
}

void BluetoothSocketAndroid::Connect(base::OnceClosure success_callback,
                                     ErrorCompletionCallback error_callback) {
  socket_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BluetoothSocketAndroid::DoConnect, this,
                     std::move(success_callback), std::move(error_callback)));
}

void BluetoothSocketAndroid::DoConnect(base::OnceClosure success_callback,
                                       ErrorCompletionCallback error_callback) {
  CHECK(socket_thread_->task_runner()->RunsTasksInCurrentSequence());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  Outcome outcome(
      Java_ChromeBluetoothSocket_connect(AttachCurrentThread(), j_socket_));
  if (!outcome) {
    DispatchErrorCallback(std::move(error_callback), outcome);
    return;
  }

  base::Thread::Options thread_options;
  thread_options.message_pump_type = base::MessagePumpType::IO;
  receiving_thread_ =
      std::make_unique<base::Thread>("BluetoothSocketReceivingThread");
  receiving_thread_->StartWithOptions(std::move(thread_options));

  ui_task_runner_->PostTask(FROM_HERE, std::move(success_callback));
}

void BluetoothSocketAndroid::Disconnect(base::OnceClosure success_callback) {
  socket_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&BluetoothSocketAndroid::DoDisconnect, this,
                                std::move(success_callback)));
}

void BluetoothSocketAndroid::DoDisconnect(base::OnceClosure success_callback) {
  CHECK(socket_thread_->task_runner()->RunsTasksInCurrentSequence());

  Java_ChromeBluetoothSocket_close(AttachCurrentThread(), j_socket_);

  receiving_thread_->Stop();
  receiving_thread_.reset();

  ui_task_runner_->PostTask(FROM_HERE, std::move(success_callback));
}

void BluetoothSocketAndroid::Receive(
    int buffer_size,
    ReceiveCompletionCallback success_callback,
    ReceiveErrorCompletionCallback error_callback) {
  if (!receiving_thread_) {
    std::move(error_callback).Run(ErrorReason::kDisconnected, "Not connected");
    return;
  }

  receiving_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BluetoothSocketAndroid::DoReceive, this,
                     static_cast<size_t>(buffer_size),
                     std::move(success_callback), std::move(error_callback)));
}

void BluetoothSocketAndroid::DoReceive(
    size_t buffer_size,
    ReceiveCompletionCallback success_callback,
    ReceiveErrorCompletionCallback error_callback) {
  CHECK(receiving_thread_->task_runner()->RunsTasksInCurrentSequence());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  JNIEnv* env = AttachCurrentThread();

  if (!Java_ChromeBluetoothSocket_isConnected(env, j_socket_)) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback),
                                  ErrorReason::kDisconnected, "Not connected"));
    return;
  }

  base::android::ScopedJavaLocalRef<jbyteArray> j_buffer(
      env, env->NewByteArray(buffer_size));
  base::android::CheckException(env);
  CHECK(j_buffer.obj());

  Outcome outcome(Java_ChromeBluetoothSocket_receive(
      env, j_socket_, j_buffer, /*offset=*/0, buffer_size));
  if (!outcome) {
    DispatchErrorCallback(
        base::BindOnce(std::move(error_callback), ErrorReason::kSystemError),
        outcome);
    return;
  }

  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(buffer_size);
  size_t bytes_copied =
      base::android::JavaByteArrayToByteSpan(env, j_buffer, buffer->span());
  CHECK_EQ(bytes_copied, buffer_size);

  int bytes_read = outcome.GetIntResult();
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(success_callback), bytes_read, buffer));
}

void BluetoothSocketAndroid::Send(scoped_refptr<net::IOBuffer> buffer,
                                  int buffer_size,
                                  SendCompletionCallback success_callback,
                                  ErrorCompletionCallback error_callback) {
  socket_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BluetoothSocketAndroid::DoSend, this, std::move(buffer),
                     static_cast<size_t>(buffer_size),
                     std::move(success_callback), std::move(error_callback)));
}

void BluetoothSocketAndroid::DoSend(scoped_refptr<net::IOBuffer> buffer,
                                    size_t buffer_size,
                                    SendCompletionCallback success_callback,
                                    ErrorCompletionCallback error_callback) {
  DCHECK(socket_thread_->task_runner()->RunsTasksInCurrentSequence());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  JNIEnv* env = AttachCurrentThread();
  if (!Java_ChromeBluetoothSocket_isConnected(env, j_socket_)) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback), "Not connected"));
    return;
  }

  base::android::ScopedJavaLocalRef<jbyteArray> j_buffer =
      base::android::ToJavaByteArray(env, buffer->span());
  Outcome outcome(Java_ChromeBluetoothSocket_send(env, j_socket_, j_buffer,
                                                  /*offset=*/0, buffer_size));
  if (!outcome) {
    DispatchErrorCallback(std::move(error_callback), outcome);
    return;
  }

  int bytes_sent = outcome.GetIntResult();
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(success_callback), bytes_sent));
}

void BluetoothSocketAndroid::Accept(AcceptCompletionCallback success_callback,
                                    ErrorCompletionCallback error_callback) {
  // Android provides BluetoothServerSocket to accept incoming connection
  // requests instead of using BluetoothSocket.
  NOTREACHED();
}

}  // namespace device
