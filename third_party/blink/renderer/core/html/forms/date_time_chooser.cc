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

#include "build/build_config.h"
#include "third_party/blink/public/mojom/choosers/date_time_chooser.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

namespace blink {

DateTimeChooserParameters::DateTimeChooserParameters() = default;

DateTimeChooserParameters::~DateTimeChooserParameters() = default;

DateTimeChooser::~DateTimeChooser() = default;

// static
bool DateTimeChooser::ShouldSubfieldsBeFocusable(LocalFrame* frame) {
  if (frame && frame->GetSettings() &&
      frame->GetSettings()
          ->DateTimeSubfieldsFocusableForTesting()
          .has_value()) {
    return *frame->GetSettings()->DateTimeSubfieldsFocusableForTesting();
  }
#if BUILDFLAG(IS_ANDROID)
  // Ideally we would look at something like the pointer type, but that didn't
  // work out because some mobile devices were reporting unexpected pointer
  // types. See http://crbug.com/567232773
  return base::android::device_info::is_desktop();
#else
  return true;
#endif
}

}  // namespace blink
