// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/wake_lock/power_save_blocker/power_save_blocker.h"

#include "base/android/jni_android.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/android/view_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/device/wake_lock/power_save_blocker/jni_headers/PowerSaveBlocker_jni.h"

namespace device {

using base::android::AttachCurrentThread;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

class PowerSaveBlocker::Delegate {
 public:
  Delegate();

  Delegate(const Delegate&) = delete;
  Delegate& operator=(const Delegate&) = delete;

  ~Delegate();

  // Does the actual work to apply or remove the desired power save block.
  void ApplyBlock(ScopedJavaGlobalRef<jobject> container_view);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_power_save_blocker_;

  SEQUENCE_CHECKER(sequence_checker_);

  size_t block_count_ = 0;
};

PowerSaveBlocker::Delegate::Delegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  JNIEnv* env = AttachCurrentThread();
  java_power_save_blocker_.Reset(Java_PowerSaveBlocker_create(env));
}

PowerSaveBlocker::Delegate::~Delegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ScopedJavaLocalRef<jobject> obj(java_power_save_blocker_);
  for (size_t i = 0; i < block_count_; ++i) {
    Java_PowerSaveBlocker_removeBlock(AttachCurrentThread(), obj);
  }
}

void PowerSaveBlocker::Delegate::ApplyBlock(
    ScopedJavaGlobalRef<jobject> container_view) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ScopedJavaLocalRef<jobject> obj(java_power_save_blocker_);
  if (container_view.is_null()) {
    return;
  }

  Java_PowerSaveBlocker_applyBlock(AttachCurrentThread(), obj, container_view);
  block_count_++;
}

PowerSaveBlocker::PowerSaveBlocker(
    mojom::WakeLockType type,
    mojom::WakeLockReason reason,
    const std::string& description,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : delegate_(ui_task_runner) {
  // Don't support PreventAppSuspension.
}

PowerSaveBlocker::~PowerSaveBlocker() = default;

void PowerSaveBlocker::InitDisplaySleepBlocker(ui::ViewAndroid* view_android) {
  DCHECK(view_android);
  delegate_.AsyncCall(&PowerSaveBlocker::Delegate::ApplyBlock)
      .WithArgs(ScopedJavaGlobalRef<jobject>(view_android->GetContainerView()));
}

}  // namespace device

DEFINE_JNI(PowerSaveBlocker)
