// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_AX_CONTEXT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_AX_CONTEXT_H_

#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"

namespace ui {
class AXMode;
}  // namespace ui

namespace blink {

class AXContext;

// An instance of this class, while kept alive, enables accessibility
// support for the given document.
class WebAXContext {
 public:
  BLINK_EXPORT explicit WebAXContext(WebDocument document,
                                     const ui::AXMode& mode);
  BLINK_EXPORT ~WebAXContext();

  // Returns the root element of the document's accessibility tree.
  BLINK_EXPORT WebAXObject Root() const;

  BLINK_EXPORT const ui::AXMode& GetAXMode() const;

 private:
  std::unique_ptr<AXContext> private_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_AX_CONTEXT_H_
