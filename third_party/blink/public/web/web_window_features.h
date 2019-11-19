/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_WINDOW_FEATURES_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_WINDOW_FEATURES_H_

namespace blink {

struct WebWindowFeatures {
  int x = 0;
  bool x_set = false;
  int y = 0;
  bool y_set = false;
  int width = 0;
  bool width_set = false;
  int height = 0;
  bool height_set = false;

  bool menu_bar_visible = true;
  bool status_bar_visible = true;
  // This can be set based on "locationbar" or "toolbar" in a window features
  // string, we don't distinguish between the two.
  bool tool_bar_visible = true;
  bool scrollbars_visible = true;

  // The members above this line are transferred through mojo
  // in the form of |struct WindowFeatures| defined in window_features.mojom,
  // to be used across process boundaries.
  // Below members are the ones not transferred through mojo.
  bool resizable = true;

  bool noopener = false;
  bool noreferrer = false;
  bool background = false;
  bool persistent = false;
};

}  // namespace blink

#endif
