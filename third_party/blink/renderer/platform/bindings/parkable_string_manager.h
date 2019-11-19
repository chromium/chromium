// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_PARKABLE_STRING_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_PARKABLE_STRING_MANAGER_H_

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_provider.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ParkableString;

PLATFORM_EXPORT extern const base::Feature kCompressParkableStrings;

class PLATFORM_EXPORT ParkableStringManagerDumpProvider
    : public base::trace_event::MemoryDumpProvider {
  USING_FAST_MALLOC(ParkableStringManagerDumpProvider);

 public:
  static ParkableStringManagerDumpProvider* Instance();
  ~ParkableStringManagerDumpProvider() override;

  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs&,
                    base::trace_event::ProcessMemoryDump*) override;

 private:
  ParkableStringManagerDumpProvider();

  DISALLOW_COPY_AND_ASSIGN(ParkableStringManagerDumpProvider);
};

// Manages all the ParkableStrings, and parks eligible strings after the
// renderer has been backgrounded.
// Main Thread only.
class PLATFORM_EXPORT ParkableStringManager {
  USING_FAST_MALLOC(ParkableStringManager);

 public:
  struct Statistics;

  static ParkableStringManager& Instance();
  ~ParkableStringManager();

  void SetRendererBackgrounded(bool backgrounded);
  bool IsRendererBackgrounded() const;
  void PurgeMemory();
  // Number of parked and unparked strings. Public for testing.
  size_t Size() const;

  bool OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd);

  // Whether a string is parkable or not. Can be called from any thread.
  static bool ShouldPark(const StringImpl& string);

  // Public for testing.
  constexpr static int kAgingIntervalInSeconds = 2;

 private:
  friend class ParkableString;
  friend class ParkableStringImpl;
  struct ParkableStringImplHash;
  struct ParkableStringImplTranslator;

  scoped_refptr<ParkableStringImpl> Add(scoped_refptr<StringImpl>&&);
  void Remove(ParkableStringImpl*);

  void OnParked(ParkableStringImpl*);
  void OnUnparked(ParkableStringImpl*);

  void ParkAll(ParkableStringImpl::ParkingMode mode);
  void RecordStatisticsAfter5Minutes() const;
  void AgeStringsAndPark();
  void ScheduleAgingTaskIfNeeded();
  void RecordUnparkingTime(base::TimeDelta);
  void RecordParkingThreadTime(base::TimeDelta parking_thread_time) {
    total_parking_thread_time_ += parking_thread_time;
  }
  Vector<ParkableStringImpl*> GetUnparkedStrings() const;
  Statistics ComputeStatistics() const;

  void ResetForTesting();

  ParkableStringManager();

  bool backgrounded_;
  bool has_pending_aging_task_;
  bool has_posted_unparking_time_accounting_task_;
  bool did_register_memory_pressure_listener_;
  base::TimeDelta total_unparking_time_;
  base::TimeDelta total_parking_thread_time_;
  WTF::HashSet<ParkableStringImpl*, ParkableStringImplHash> unparked_strings_;
  WTF::HashSet<ParkableStringImpl*, ParkableStringImplHash> parked_strings_;

  friend class ParkableStringTest;
  FRIEND_TEST_ALL_PREFIXES(ParkableStringTest, SynchronousCompression);
  DISALLOW_COPY_AND_ASSIGN(ParkableStringManager);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_PARKABLE_STRING_MANAGER_H_
