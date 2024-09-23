// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/win/dwrite_font_format_support.h"

#include "skia/ext/font_utils.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkFontStyle.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {
namespace {

bool DWriteVersionSupportsVariationsImpl() {
  sk_sp<SkFontMgr> fm = skia::DefaultFontMgr();
  sk_sp<SkTypeface> probe_typeface =
      fm->legacyMakeTypeface(nullptr, SkFontStyle());
  if (!probe_typeface) {
    return false;
  }
  int variation_design_position_result =
      probe_typeface->getVariationDesignPosition(nullptr, 0);
  return variation_design_position_result > -1;
}

class DWriteVersionSupportsVariationsChecker {
 public:
  DWriteVersionSupportsVariationsChecker()
      : value_(DWriteVersionSupportsVariationsImpl()) {}
  ~DWriteVersionSupportsVariationsChecker() = default;
  DWriteVersionSupportsVariationsChecker(
      const DWriteVersionSupportsVariationsChecker&) = delete;
  DWriteVersionSupportsVariationsChecker& operator=(
      const DWriteVersionSupportsVariationsChecker&) = delete;

  bool value() const { return value_; }

 private:
  const bool value_;
};

}  // namespace

bool DWriteVersionSupportsVariations() {
  // We're instantiating a default typeface. The usage of legacyMakeTypeface()
  // is intentional here to access a basic default font. Its implementation will
  // ultimately use the first font face from the first family in the system font
  // collection. Use this probe type face to ask Skia for the variation design
  // position. Internally, Skia then tests whether the DWrite interfaces for
  // accessing variable font information are available, in other words, if
  // QueryInterface for IDWriteFontFace5 succeeds. If it doesn't it returns -1
  // and we know DWrite on this system does not support OpenType variations. If
  // the response is 0 or larger, it means, DWrite was able to determine if this
  // is a variable font or not and Variations are supported.
  //
  // We are using ThreadSpecific here to avoid a deadlock (crbug.com/344108551).
  // SkFontMgr::legacyMakeTypeface() may call a synchronous IPC to the browser
  // process, requiring to bind a DWriteFontProxy mojo handle on the main thread
  // for non-main thread calls. If `variations_supported` were a process-wide
  // static boolean variable and we used exclusive control to ensure
  // SkFontMgr::legacyMakeTypeface() is called only once when called from
  // multiple threads, a deadlock could occur in the following scenario: the
  // main thread and a background thread call this function simultaneously, the
  // background thread is slightly faster, and the IPC ends up being called from
  // the background thread. To avoid this, we made `variations_supported`
  // ThreadSpecific so that DWriteVersionSupportsVariationsImpl() is called only
  // once in each thread.
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      ThreadSpecific<DWriteVersionSupportsVariationsChecker>,
      variations_supported, ());
  return variations_supported->value();
}
}  // namespace blink
