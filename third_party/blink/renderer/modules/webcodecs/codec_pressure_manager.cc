// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_manager.h"

#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_gauge.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

CodecPressureManager::CodecPressureManager(
    ReclaimableCodec::CodecType codec_type,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : codec_type_(codec_type) {
  auto pressure_threshold_changed_cb =
      [](CrossThreadWeakPersistent<CodecPressureManager> self,
         scoped_refptr<base::SequencedTaskRunner> task_runner,
         bool global_pressure_exceeded) {
        // Accessing |self| is not thread safe. Even if it is thread unsafe,
        // checking for |!self| can definitively tell us if the object has
        // already been GC'ed, so we can exit early. Otherwise, we always post
        // to |task_runner|, where it will be safe to use |self|, since it's
        // in the same sequence that created |self|.
        if (!self)
          return;

        // Always post this change, to guarantee ordering if this callback
        // is run from different threads.
        DCHECK(task_runner);
        PostCrossThreadTask(
            *task_runner, FROM_HERE,
            CrossThreadBindOnce(
                &CodecPressureManager::OnGlobalPressureThresholdChanged, self,
                global_pressure_exceeded));
      };

  CodecPressureGauge::RegistrationResult result =
      GetCodecPressureGauge().RegisterPressureCallback(
          ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
              pressure_threshold_changed_cb,
              WrapCrossThreadWeakPersistent(this), task_runner)));

  pressure_callback_id_ = result.first;
  global_pressure_exceeded_ = result.second;
}

CodecPressureGauge& CodecPressureManager::GetCodecPressureGauge() {
  return CodecPressureGauge::GetInstance(codec_type_);
}

void CodecPressureManager::AddCodec(ReclaimableCodec* codec) {
  DCHECK(manager_registered_);
  DCHECK(codec->is_applying_codec_pressure());

  DCHECK(!codecs_with_pressure_.Contains(codec));
  codecs_with_pressure_.insert(codec);

  ++local_codec_pressure_;
  GetCodecPressureGauge().Increment();

  codec->SetGlobalPressureExceededFlag(global_pressure_exceeded_);
}

void CodecPressureManager::RemoveCodec(ReclaimableCodec* codec) {
  DCHECK(manager_registered_);

  DCHECK(codecs_with_pressure_.Contains(codec));
  codecs_with_pressure_.erase(codec);

  DCHECK(local_codec_pressure_);
  --local_codec_pressure_;
  GetCodecPressureGauge().Decrement();

  // |codec| is responsible for clearing its own global pressure exceeded flag.
}

void CodecPressureManager::OnCodecDisposed(ReclaimableCodec* codec) {
  DCHECK(codec->is_applying_codec_pressure());

  if (!manager_registered_) {
    // |this|'s pre-finalizer (UnregisterManager()) could have been called
    // before leftover ReclaimableCodec's pre-finalizers were called.
    // This shouldn't happen often, but it might if |this| and codecs are
    // prefinalized in the same GC run.
    DCHECK_EQ(local_codec_pressure_, 0u);
    return;
  }

  // The GC should have removed |codec| from |codecs_with_pressure_|.
  DCHECK(!codecs_with_pressure_.Contains(codec));

  DCHECK(local_codec_pressure_);
  --local_codec_pressure_;
  GetCodecPressureGauge().Decrement();
}

void CodecPressureManager::OnGlobalPressureThresholdChanged(
    bool pressure_threshold_exceeded) {
  DCHECK_NE(global_pressure_exceeded_, pressure_threshold_exceeded);
  global_pressure_exceeded_ = pressure_threshold_exceeded;

  for (auto codec : codecs_with_pressure_)
    codec->SetGlobalPressureExceededFlag(global_pressure_exceeded_);
}

void CodecPressureManager::UnregisterManager() {
  if (!manager_registered_)
    return;

  GetCodecPressureGauge().UnregisterPressureCallback(pressure_callback_id_,
                                                     local_codec_pressure_);
  codecs_with_pressure_.clear();

  local_codec_pressure_ = 0u;

  manager_registered_ = false;
}

void CodecPressureManager::Trace(Visitor* visitor) const {
  visitor->Trace(codecs_with_pressure_);
}

}  // namespace blink
