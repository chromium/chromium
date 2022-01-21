// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_CODEC_PRESSURE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_CODEC_PRESSURE_MANAGER_H_

#include "base/sequence_checker.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webcodecs/reclaimable_codec.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

// Keeps track of the number of pressuring codecs in an ExecutionContext, and
// notifies these codecs when a pressure threshold has been crossed.
class MODULES_EXPORT CodecPressureManager
    : public GarbageCollected<CodecPressureManager> {
 public:
  CodecPressureManager();

  // Disable copy and assign.
  CodecPressureManager(const CodecPressureManager&) = delete;
  CodecPressureManager& operator=(const CodecPressureManager&) = delete;

  // Adds or removes a codec, potentially causing global pressure flags to be
  // set/unset.
  void AddCodec(ReclaimableCodec*);
  void RemoveCodec(ReclaimableCodec*);

  // Called when a codec is disposed without having called RemoveCodec() first.
  // This can happen when a codec is garbage collected without having been
  // closed or reclaimed first.
  void OnCodecDisposed(ReclaimableCodec*);

  size_t pressure_for_testing() { return codecs_with_pressure_.size(); }

  void set_pressure_threshold_for_testing(size_t threshold) {
    pressure_threshold_ = threshold;
  }

  void Trace(Visitor*) const;

 private:
  friend class CodecPressureManagerTest;

  using CodecSet = HeapHashSet<WeakMember<ReclaimableCodec>>;

  void MaybeSetGlobalPressureFlags(ReclaimableCodec*);
  void MaybeClearGlobalPressureFlags();

  bool global_pressure_exceeded_ = false;

  size_t pressure_threshold_;

  CodecSet codecs_with_pressure_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_CODEC_PRESSURE_MANAGER_H_
