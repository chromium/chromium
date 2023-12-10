// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/string_util_icu.h"

#include <memory>

#include "base/memory/singleton.h"
#include "base/strings/string_util.h"
#include "third_party/icu/source/common/unicode/uniset.h"

namespace device {

namespace {

class GraphicCharacters {
 public:
  static GraphicCharacters* GetInstance() {
    return base::Singleton<GraphicCharacters, base::LeakySingletonTraits<
                                                  GraphicCharacters>>::get();
  }

  GraphicCharacters(const GraphicCharacters&) = delete;
  GraphicCharacters& operator=(const GraphicCharacters&) = delete;

  bool HasGraphicCharacter(std::string_view s) {
    int32_t length = graphic_->spanUTF8(
        s.data(), s.size(), USetSpanCondition::USET_SPAN_NOT_CONTAINED);
    return static_cast<size_t>(length) != s.size();
  }

 private:
  friend struct base::DefaultSingletonTraits<GraphicCharacters>;

  GraphicCharacters();

  // set of graphic characters.
  std::unique_ptr<icu::UnicodeSet> graphic_;
};

GraphicCharacters::GraphicCharacters() {
  UErrorCode graphic_status = U_ZERO_ERROR;

  // The set of Unicode Graphic Characters as defined by
  // http://www.unicode.org/reports/tr18/#graph
  // This set is composed of the characters not included in the following
  // sets:
  // - Whitespace (WSpace)
  // - gc=Control (Cc)
  // - gc=Surrogate (Cs)
  // - gc=Unassigned (Cn)
  graphic_ = std::make_unique<icu::UnicodeSet>(
      UNICODE_STRING_SIMPLE("[:graph:]"), graphic_status);
  DCHECK(U_SUCCESS(graphic_status));

  graphic_->freeze();
}

}  // namespace

bool HasGraphicCharacter(std::string_view s) {
  DCHECK(base::IsStringUTF8(s));
  return GraphicCharacters::GetInstance()->HasGraphicCharacter(s);
}

}  // namespace device
