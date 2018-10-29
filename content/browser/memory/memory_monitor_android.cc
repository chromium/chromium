// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_monitor_android.h"

#include "base/android/jni_android.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ptr_util.h"
#include "jni/MemoryMonitorAndroid_jni.h"

namespace content {

namespace {

const size_t kMBShift = 20;

void RegisterComponentCallbacks() {
  Java_MemoryMonitorAndroid_registerComponentCallbacks(
      base::android::AttachCurrentThread());
}

}

// An implementation of MemoryMonitorAndroid::Delegate using the Android APIs.
class MemoryMonitorAndroidDelegateImpl : public MemoryMonitorAndroid::Delegate {
 public:
  MemoryMonitorAndroidDelegateImpl() {}
  ~MemoryMonitorAndroidDelegateImpl() override {}

  using MemoryInfo = MemoryMonitorAndroid::MemoryInfo;
  void GetMemoryInfo(MemoryInfo* out) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryMonitorAndroidDelegateImpl);
};

void MemoryMonitorAndroidDelegateImpl::GetMemoryInfo(MemoryInfo* out) {
  DCHECK(out);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MemoryMonitorAndroid_getMemoryInfo(env, reinterpret_cast<intptr_t>(out));
}

// Called by JNI to populate ActivityManager.MemoryInfo.
static void JNI_MemoryMonitorAndroid_GetMemoryInfoCallback(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& clazz,
    jlong avail_mem,
    jboolean low_memory,
    jlong threshold,
    jlong total_mem,
    jlong out_ptr) {
  DCHECK(out_ptr);
  MemoryMonitorAndroid::MemoryInfo* info =
      reinterpret_cast<MemoryMonitorAndroid::MemoryInfo*>(out_ptr);
  info->avail_mem = avail_mem;
  info->low_memory = low_memory;
  info->threshold = threshold;
  info->total_mem = total_mem;
}

// The maximum level of onTrimMemory (TRIM_MEMORY_COMPLETE).
const int kTrimMemoryLevelMax = 80;
const int kTrimMemoryRunningCritical = 15;

// Called by JNI.
static void JNI_MemoryMonitorAndroid_OnTrimMemory(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    jint level) {
  DCHECK(level >= 0 && level <= kTrimMemoryLevelMax);

  if (level >= kTrimMemoryRunningCritical) {
    base::MemoryPressureListener::NotifyMemoryPressure(
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  }
}

// static
std::unique_ptr<MemoryMonitorAndroid> MemoryMonitorAndroid::Create() {
  auto delegate = base::WrapUnique(new MemoryMonitorAndroidDelegateImpl);
  return base::WrapUnique(new MemoryMonitorAndroid(std::move(delegate)));
}

MemoryMonitorAndroid::MemoryMonitorAndroid(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK(delegate_.get());
  RegisterComponentCallbacks();
}

MemoryMonitorAndroid::~MemoryMonitorAndroid() {}

int MemoryMonitorAndroid::GetFreeMemoryUntilCriticalMB() {
  MemoryInfo info;
  GetMemoryInfo(&info);
  return (info.avail_mem - info.threshold) >> kMBShift;
}

void MemoryMonitorAndroid::GetMemoryInfo(MemoryInfo* out) {
  delegate_->GetMemoryInfo(out);
}

// Implementation of a factory function defined in memory_monitor.h.
std::unique_ptr<MemoryMonitor> CreateMemoryMonitor() {
  return MemoryMonitorAndroid::Create();
}

}  // namespace content
