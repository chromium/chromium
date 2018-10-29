// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_PARKABLE_STRING_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_PARKABLE_STRING_MANAGER_H_

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ParkableString;
class ParkableStringImpl;

const base::Feature kCompressParkableStringsInBackground{
    "CompressParkableStringsInBackground", base::FEATURE_DISABLED_BY_DEFAULT};

// Manages all the ParkableStrings, and parks eligible strings after the
// renderer has been backgrounded.
// Main Thread only.
class PLATFORM_EXPORT ParkableStringManager {
  USING_FAST_MALLOC(ParkableStringManager);

 public:
  static ParkableStringManager& Instance();
  ~ParkableStringManager();

  void SetRendererBackgrounded(bool backgrounded);
  bool IsRendererBackgrounded() const;

  // Whether a string is parkable or not. Can be called from any thread.
  static bool ShouldPark(const StringImpl& string);

 private:
  friend class ParkableString;
  friend class ParkableStringImpl;

  scoped_refptr<ParkableStringImpl> Add(scoped_refptr<StringImpl>&&);
  void Remove(ParkableStringImpl*, StringImpl*);

  void OnParked(ParkableStringImpl*, StringImpl*);
  void OnUnparked(ParkableStringImpl*, StringImpl*);

  void ParkAllIfRendererBackgrounded();
  size_t Size() const;

  ParkableStringManager();

  bool backgrounded_;
  HashMap<StringImpl*, ParkableStringImpl*, PtrHash<StringImpl>>
      unparked_strings_;
  HashSet<ParkableStringImpl*, PtrHash<ParkableStringImpl>> parked_strings_;

  FRIEND_TEST_ALL_PREFIXES(ParkableStringTest, ManagerSimple);
  FRIEND_TEST_ALL_PREFIXES(ParkableStringTest, ManagerMultipleStrings);
  DISALLOW_COPY_AND_ASSIGN(ParkableStringManager);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_PARKABLE_STRING_MANAGER_H_
