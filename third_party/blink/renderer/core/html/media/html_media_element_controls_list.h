// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_HTML_MEDIA_ELEMENT_CONTROLS_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_HTML_MEDIA_ELEMENT_CONTROLS_LIST_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class HTMLMediaElement;

class CORE_EXPORT HTMLMediaElementControlsList final : public DOMTokenList {
 public:
  explicit HTMLMediaElementControlsList(HTMLMediaElement*);

  // Whether the list dictates to hide a certain control.
  bool ShouldHideDownload() const;
  bool ShouldHideFullscreen() const;
  bool ShouldHideRemotePlayback() const;

 private:
  bool ValidateTokenValue(const AtomicString&, ExceptionState&) const override;
};

}  // namespace blink

#endif
