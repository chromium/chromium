#!/bin/bash

# This script renames all the functions and the macros defined in
# absl/base/dynamic_annotations.{h,cc} and absl/base/thread_annotations.h.
#
# Chromium's dynamic_annotations live in //base/third_party/dynamic_annotations
# and its //base contains a copy of thread_annotations.h which conflict with
# Abseil's versions (ODR violations and macro clashing).
# In order to avoid problems in Chromium, this copy of Abseil has its own
# dynamic_annotations and thread_annotations renamed.

# -------------------------- dynamic_annotations -------------------------
for w in \
  AnnotateBarrierDestroy \
  AnnotateBarrierInit \
  AnnotateBarrierWaitAfter \
  AnnotateBarrierWaitBefore \
  AnnotateBenignRace \
  AnnotateBenignRaceSized \
  AnnotateCondVarSignal \
  AnnotateCondVarSignalAll \
  AnnotateCondVarWait \
  AnnotateEnableRaceDetection \
  AnnotateExpectRace \
  AnnotateFlushExpectedRaces \
  AnnotateFlushState \
  AnnotateHappensAfter \
  AnnotateHappensBefore \
  AnnotateIgnoreReadsBegin \
  AnnotateIgnoreReadsEnd \
  AnnotateIgnoreSyncBegin \
  AnnotateIgnoreSyncEnd \
  AnnotateIgnoreWritesBegin \
  AnnotateIgnoreWritesEnd \
  AnnotateMemoryIsInitialized \
  AnnotateMemoryIsUninitialized \
  AnnotateMutexIsNotPHB \
  AnnotateMutexIsUsedAsCondVar \
  AnnotateNewMemory \
  AnnotateNoOp \
  AnnotatePCQCreate \
  AnnotatePCQDestroy \
  AnnotatePCQGet \
  AnnotatePCQPut \
  AnnotatePublishMemoryRange \
  AnnotateRWLockAcquired \
  AnnotateRWLockCreate \
  AnnotateRWLockCreateStatic \
  AnnotateRWLockDestroy \
  AnnotateRWLockReleased \
  AnnotateThreadName \
  AnnotateTraceMemory \
  AnnotateUnpublishMemoryRange \
  GetRunningOnValgrind \
  RunningOnValgrind \
  StaticAnnotateIgnoreReadsBegin \
  StaticAnnotateIgnoreReadsEnd \
  StaticAnnotateIgnoreWritesBegin \
  StaticAnnotateIgnoreWritesEnd \
  ValgrindSlowdown \
; do
  find absl/ -type f -exec sed -i "s/\b$w\b/Absl$w/g" {} \;
done

for w in \
  ADDRESS_SANITIZER_REDZONE \
  ANNOTALYSIS_ENABLED \
  ANNOTATE_BARRIER_DESTROY \
  ANNOTATE_BARRIER_INIT \
  ANNOTATE_BARRIER_WAIT_AFTER \
  ANNOTATE_BARRIER_WAIT_BEFORE \
  ANNOTATE_BENIGN_RACE \
  ANNOTATE_BENIGN_RACE_SIZED \
  ANNOTATE_BENIGN_RACE_STATIC \
  ANNOTATE_CONDVAR_LOCK_WAIT \
  ANNOTATE_CONDVAR_SIGNAL \
  ANNOTATE_CONDVAR_SIGNAL_ALL \
  ANNOTATE_CONDVAR_WAIT \
  ANNOTATE_CONTIGUOUS_CONTAINER \
  ANNOTATE_ENABLE_RACE_DETECTION \
  ANNOTATE_EXPECT_RACE \
  ANNOTATE_FLUSH_EXPECTED_RACES \
  ANNOTATE_FLUSH_STATE \
  ANNOTATE_HAPPENS_AFTER \
  ANNOTATE_HAPPENS_BEFORE \
  ANNOTATE_IGNORE_READS_AND_WRITES_BEGIN ANNOTATE_IGNORE_READS_AND_WRITES_END \
  ANNOTATE_IGNORE_READS_BEGIN \
  ANNOTATE_IGNORE_READS_END \
  ANNOTATE_IGNORE_SYNC_BEGIN \
  ANNOTATE_IGNORE_SYNC_END \
  ANNOTATE_IGNORE_WRITES_BEGIN \
  ANNOTATE_IGNORE_WRITES_END \
  ANNOTATE_MEMORY_IS_INITIALIZED \
  ANNOTATE_MEMORY_IS_UNINITIALIZED \
  ANNOTATE_MUTEX_IS_USED_AS_CONDVAR \
  ANNOTATE_NEW_MEMORY \
  ANNOTATE_NOT_HAPPENS_BEFORE_MUTEX \
  ANNOTATE_NO_OP \
  ANNOTATE_PCQ_CREATE ANNOTATE_PCQ_DESTROY \
  ANNOTATE_PCQ_GET ANNOTATE_PCQ_PUT \
  ANNOTATE_PUBLISH_MEMORY_RANGE \
  ANNOTATE_PURE_HAPPENS_BEFORE_MUTEX \
  ANNOTATE_RWLOCK_ACQUIRED \
  ANNOTATE_RWLOCK_CREATE \
  ANNOTATE_RWLOCK_CREATE_STATIC \
  ANNOTATE_RWLOCK_DESTROY \
  ANNOTATE_RWLOCK_RELEASED \
  ANNOTATE_SWAP_MEMORY_RANGE \
  ANNOTATE_THREAD_NAME \
  ANNOTATE_TRACE_MEMORY \
  ANNOTATE_UNPROTECTED_READ \
  ANNOTATE_UNPUBLISH_MEMORY_RANGE \
  ANNOTATIONS_ENABLED \
  ATTRIBUTE_IGNORE_READS_BEGIN \
  ATTRIBUTE_IGNORE_READS_END \
  DYNAMIC_ANNOTATIONS_ATTRIBUTE_WEAK \
  DYNAMIC_ANNOTATIONS_ENABLED \
  DYNAMIC_ANNOTATIONS_EXTERNAL_IMPL \
  DYNAMIC_ANNOTATIONS_GLUE \
  DYNAMIC_ANNOTATIONS_GLUE0 \
  DYNAMIC_ANNOTATIONS_IMPL \
  DYNAMIC_ANNOTATIONS_NAME \
  DYNAMIC_ANNOTATIONS_PREFIX \
  DYNAMIC_ANNOTATIONS_PROVIDE_RUNNING_ON_VALGRIND \
  DYNAMIC_ANNOTATIONS_WANT_ATTRIBUTE_WEAK \
; do
  find absl/ -type f -exec sed -i "s/\b$w\b/ABSL_$w/g" {} \;
done

# -------------------------- thread_annotations -------------------------

for w in \
  ts_unchecked_read \
; do
  find absl/ -type f -exec sed -i "s/\b$w\b/absl_$w/g" {} \;
done

for w in \
  THREAD_ANNOTATION_ATTRIBUTE__ \
  GUARDED_BY \
  PT_GUARDED_BY \
  ACQUIRED_AFTER \
  ACQUIRED_BEFORE \
  EXCLUSIVE_LOCKS_REQUIRED \
  SHARED_LOCKS_REQUIRED \
  LOCKS_EXCLUDED \
  LOCK_RETURNED \
  LOCKABLE \
  SCOPED_LOCKABLE \
  EXCLUSIVE_LOCK_FUNCTION \
  SHARED_LOCK_FUNCTION \
  UNLOCK_FUNCTION \
  EXCLUSIVE_TRYLOCK_FUNCTION \
  SHARED_TRYLOCK_FUNCTION \
  ASSERT_EXCLUSIVE_LOCK \
  ASSERT_SHARED_LOCK \
  NO_THREAD_SAFETY_ANALYSIS \
  TS_UNCHECKED \
  TS_FIXME \
  NO_THREAD_SAFETY_ANALYSIS_FIXME \
  GUARDED_BY_FIXME \
  TS_UNCHECKED_READ \
; do
  find absl/ -type f -exec sed -i "s/\b$w\b/ABSL_$w/g" {} \;
done
