// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/wake_lock/power_save_blocker/power_save_blocker.h"

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "services/device/wake_lock/power_save_blocker/jni_headers/PowerSaveBlocker_jni.h"
#include "ui/android/view_android.h"

namespace device {

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

class PowerSaveBlocker::Delegate
    : public base::RefCountedThreadSafe<PowerSaveBlocker::Delegate> {
 public:
  Delegate(scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  // Does the actual work to apply or remove the desired power save block.
  void ApplyBlock(ui::ViewAndroid* view_android);
  void RemoveBlock();

 private:
  friend class base::RefCountedThreadSafe<Delegate>;
  ~Delegate();

  base::android::ScopedJavaGlobalRef<jobject> java_power_save_blocker_;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

PowerSaveBlocker::Delegate::Delegate(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : ui_task_runner_(ui_task_runner) {
  JNIEnv* env = AttachCurrentThread();
  java_power_save_blocker_.Reset(Java_PowerSaveBlocker_create(env));
}

PowerSaveBlocker::Delegate::~Delegate() {}

void PowerSaveBlocker::Delegate::ApplyBlock(ui::ViewAndroid* view_android) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(view_android);

  ScopedJavaLocalRef<jobject> obj(java_power_save_blocker_);
  ScopedJavaLocalRef<jobject> container_view(view_android->GetContainerView());
  if (container_view.is_null())
    return;

  Java_PowerSaveBlocker_applyBlock(AttachCurrentThread(), obj, container_view);
}

void PowerSaveBlocker::Delegate::RemoveBlock() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  ScopedJavaLocalRef<jobject> obj(java_power_save_blocker_);
  Java_PowerSaveBlocker_removeBlock(AttachCurrentThread(), obj);
}

PowerSaveBlocker::PowerSaveBlocker(
    mojom::WakeLockType type,
    mojom::WakeLockReason reason,
    const std::string& description,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> blocking_task_runner)
    : ui_task_runner_(ui_task_runner),
      blocking_task_runner_(blocking_task_runner) {
  // Don't support PreventAppSuspension.
}

PowerSaveBlocker::~PowerSaveBlocker() {
  if (delegate_.get()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Delegate::RemoveBlock, delegate_));
  }
}

void PowerSaveBlocker::InitDisplaySleepBlocker(ui::ViewAndroid* view_android) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(view_android);

  delegate_ = new Delegate(ui_task_runner_);
  delegate_->ApplyBlock(view_android);
}

}  // namespace device
