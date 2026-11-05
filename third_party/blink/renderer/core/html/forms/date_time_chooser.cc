/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/forms/date_time_chooser.h"

#include "third_party/blink/public/mojom/choosers/date_time_chooser.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

DateTimeChooserParameters::DateTimeChooserParameters() = default;

DateTimeChooserParameters::~DateTimeChooserParameters() = default;

DateTimeChooser::~DateTimeChooser() = default;

// static
bool DateTimeChooser::ShouldSubfieldsBeFocusable(LocalFrame* frame) {
  if (!frame || !frame->GetSettings()) {
    return true;
  }

  const bool has_fine_pointer =
      frame->GetSettings()->GetAvailablePointerTypes() &
      static_cast<int>(mojom::blink::PointerType::kPointerFineType);
  const bool has_coarse_pointer =
      frame->GetSettings()->GetAvailablePointerTypes() &
      static_cast<int>(mojom::blink::PointerType::kPointerCoarseType);

  // We generally want to target mobile, which has a coarse pointer and no fine
  // pointer. If a fine pointer is present, then the user should be able to
  // click the picker icon to open the picker. If the user is using a stylus on
  // a tablet, then they should be able to click the picker icon without the
  // need for the entire control being a target to open the picker.
  return has_fine_pointer || !has_coarse_pointer;
}

}  // namespace blink
