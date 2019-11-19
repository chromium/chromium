// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_scrubbing_message_element.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_resource_loader.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

MediaControlScrubbingMessageElement::MediaControlScrubbingMessageElement(
    MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls) {
  SetShadowPseudoId(AtomicString("-internal-media-controls-scrubbing-message"));
  CreateUserAgentShadowRoot();
  SetIsWanted(false);
}

void MediaControlScrubbingMessageElement::PopulateChildren() {
  ShadowRoot* shadow_root = GetShadowRoot();

  // This stylesheet element will contain rules that are specific to the
  // scrubbing message. The shadow DOM protects these rules from bleeding
  // across to the parent DOM.
  auto* style = MakeGarbageCollected<HTMLStyleElement>(GetDocument(),
                                                       CreateElementFlags());
  style->setTextContent(
      MediaControlsResourceLoader::GetScrubbingMessageStyleSheet());
  shadow_root->ParserAppendChild(style);

  HTMLDivElement* arrow_left_div1 =
      MediaControlElementsHelper::CreateDivWithId("arrow-left1", shadow_root);
  HTMLDivElement* arrow_left_div2 =
      MediaControlElementsHelper::CreateDivWithId("arrow-left2", shadow_root);
  HTMLDivElement* message_div =
      MediaControlElementsHelper::CreateDivWithId("message", shadow_root);
  HTMLDivElement* arrow_right_div1 =
      MediaControlElementsHelper::CreateDivWithId("arrow-right1", shadow_root);
  HTMLDivElement* arrow_right_div2 =
      MediaControlElementsHelper::CreateDivWithId("arrow-right2", shadow_root);

  arrow_left_div1->SetInnerHTMLFromString(
      MediaControlsResourceLoader::GetArrowLeftSVGImage());
  arrow_left_div2->SetInnerHTMLFromString(
      MediaControlsResourceLoader::GetArrowLeftSVGImage());
  message_div->setInnerText(
      MediaElement().GetLocale().QueryString(IDS_MEDIA_SCRUBBING_MESSAGE_TEXT),
      ASSERT_NO_EXCEPTION);
  arrow_right_div1->SetInnerHTMLFromString(
      MediaControlsResourceLoader::GetArrowRightSVGImage());
  arrow_right_div2->SetInnerHTMLFromString(
      MediaControlsResourceLoader::GetArrowRightSVGImage());
}

void MediaControlScrubbingMessageElement::SetIsWanted(bool wanted) {
  // Populate the DOM on demand.
  if (wanted && !GetShadowRoot()->firstChild())
    PopulateChildren();

  MediaControlDivElement::SetIsWanted(wanted);
}

}  // namespace blink
