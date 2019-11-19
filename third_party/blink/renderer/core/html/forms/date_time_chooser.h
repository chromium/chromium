/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_DATE_TIME_CHOOSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_DATE_TIME_CHOOSER_H_

#include "base/macros.h"
#include "third_party/blink/public/mojom/choosers/date_time_chooser.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class AXObject;

struct DateTimeChooserParameters {
  DISALLOW_NEW();
  CORE_EXPORT DateTimeChooserParameters();
  CORE_EXPORT ~DateTimeChooserParameters();

  AtomicString type;
  IntRect anchor_rect_in_screen;
  // Locale name for which the chooser should be localized. This
  // might be an invalid name because it comes from HTML lang
  // attributes.
  AtomicString locale;
  double double_value;
  Vector<mojom::blink::DateTimeSuggestionPtr> suggestions;
  double minimum;
  double maximum;
  double step;
  double step_base;
  bool required;
  bool is_anchor_element_rtl;
  // The fields below are used for type="time".
  // For some locales the am/pm is the first field, so is_ampm_first informs
  // the time popup when the am/pm column should be the first one.
  bool is_ampm_first;
  bool has_ampm;
  bool has_second;
  bool has_millisecond;

 private:
  // DateTimeSuggestionPtr is not copyable.
  DISALLOW_COPY_AND_ASSIGN(DateTimeChooserParameters);
};

// For pickers like color pickers and date pickers.
class CORE_EXPORT DateTimeChooser : public GarbageCollected<DateTimeChooser> {
 public:
  virtual ~DateTimeChooser();

  virtual void EndChooser() = 0;
  // Returns a root AXObject in the DateTimeChooser if it's available.
  virtual AXObject* RootAXObject() = 0;

  virtual void Trace(Visitor* visitor) {}
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_DATE_TIME_CHOOSER_H_
