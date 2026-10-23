// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/text/capitalize.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/case_map.h"
#include "third_party/blink/renderer/platform/wtf/text/text_offset_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

// Microbenchmarks for the two implementations backing
// `text-transform: capitalize`: the legacy `blink::Capitalize()` and the
// ICU-based `CaseMap::ToTitle()`.

namespace blink {
namespace {

constexpr base::TimeDelta kMinRunTime = base::Milliseconds(600);
constexpr int kWarmupIterations = 20;

// Short Speedometer-ish English fragments: TodoMVC-style labels, table
// headers, etc. Covers the bulk of real `text-transform: capitalize` traffic.
const char* const kSpeedometerLike[] = {
    "due date",       "add item",         "complete all",
    "show active",    "clear completed",  "what needs to be done",
    "mark as done",   "edit todo",        "delete selected",
    "select all",     "sort by name",     "ascending order",
    "price per unit", "quantity on hand", "shipping address",
    "payment method",
};

Vector<String> SpeedometerInputs() {
  Vector<String> out;
  for (const char* s : kSpeedometerLike) {
    out.push_back(String::FromUtf8(s));
  }
  return out;
}

Vector<String> DutchInputs() {
  Vector<String> out;
  out.push_back(String::FromUtf8("ijsland"));
  out.push_back(String::FromUtf8("ijsselmeer"));
  out.push_back(String::FromUtf8("ijmuiden hoogovens"));
  out.push_back(String::FromUtf8("de ijzeren eeuw"));
  return out;
}

Vector<String> LongEnglishInput() {
  Vector<String> out;
  out.push_back(
      String::FromUtf8("the quick brown fox jumps over the lazy dog near the "
                       "river bank while the sun sets slowly behind the "
                       "distant mountain range"));
  return out;
}

// Two paths backing `text-transform: capitalize`:
//   kLegacy -- `blink::Capitalize()` (used when `ICUCapitalization` is off).
//   kIcu    -- `CaseMap::ToTitle()` (used when the flag is on).
enum class Path { kLegacy, kIcu };

struct Result {
  int64_t iters = 0;
  base::TimeDelta elapsed;
  double NsPerCall() const {
    return static_cast<double>(elapsed.InNanoseconds()) /
           static_cast<double>(iters);
  }
};

// Tiny checksum to keep the optimizer from eliding the work without pulling
// in google/benchmark's DoNotOptimize.
uint32_t Checksum(const String& s) {
  if (s.IsNull() || s.empty()) {
    return 0;
  }
  return s.length() ^ static_cast<uint32_t>(s[0]);
}

Result Bench(Path path,
             const AtomicString& locale,
             const Vector<String>& inputs) {
  CaseMap case_map(locale);
  const UChar kPrev = ' ';
  const UChar32 kPrev32 = ' ';
  uint32_t sink = 0;

  auto apply = [&](const String& input) {
    return path == Path::kLegacy
               ? Capitalize(input, kPrev32)
               : case_map.ToTitle(input, /*offset_map=*/nullptr, kPrev);
  };

  // Warmup: force any lazy ICU init (break iterator cache) off the clock.
  for (int i = 0; i < kWarmupIterations; ++i) {
    for (const String& input : inputs) {
      sink ^= Checksum(apply(input));
    }
  }

  int64_t iters = 0;
  const base::TimeTicks start = base::TimeTicks::Now();
  base::TimeDelta elapsed;
  do {
    for (const String& input : inputs) {
      sink ^= Checksum(apply(input));
    }
    iters += inputs.size();
    elapsed = base::TimeTicks::Now() - start;
  } while (elapsed < kMinRunTime);

  // Publish `sink` so the compiler can't drop everything.
  if (sink == 0xDEADBEEF) {
    LOG(INFO) << "sink=" << sink;
  }
  return {iters, elapsed};
}

void Report(const std::string& story, const Result& r) {
  LOG(INFO) << "[Capitalize] " << story << ": " << r.NsPerCall() << " ns/call"
            << " (" << r.iters << " iters in " << r.elapsed.InMillisecondsF()
            << " ms)";
}

TEST(CapitalizePerfTest, Speedometer_LegacyVsIcu) {
  Report("speedometer legacy (root)",
         Bench(Path::kLegacy, g_null_atom, SpeedometerInputs()));
  Report("speedometer legacy (en)",
         Bench(Path::kLegacy, AtomicString("en"), SpeedometerInputs()));
  Report("speedometer ICU    (root)",
         Bench(Path::kIcu, g_null_atom, SpeedometerInputs()));
  Report("speedometer ICU    (en)",
         Bench(Path::kIcu, AtomicString("en"), SpeedometerInputs()));
}

TEST(CapitalizePerfTest, Dutch_LegacyVsIcu) {
  Report("dutch legacy       (nl)",
         Bench(Path::kLegacy, AtomicString("nl"), DutchInputs()));
  Report("dutch ICU          (nl)",
         Bench(Path::kIcu, AtomicString("nl"), DutchInputs()));
}

TEST(CapitalizePerfTest, LongEnglish_LegacyVsIcu) {
  Report("long en legacy     (root)",
         Bench(Path::kLegacy, g_null_atom, LongEnglishInput()));
  Report("long en ICU        (root)",
         Bench(Path::kIcu, g_null_atom, LongEnglishInput()));
}

}  // namespace
}  // namespace blink
