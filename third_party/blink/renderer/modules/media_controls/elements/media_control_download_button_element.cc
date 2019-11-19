// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_download_button_element.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element_controls_list.h"
#include "third_party/blink/renderer/core/html/media/html_media_source.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

MediaControlDownloadButtonElement::MediaControlDownloadButtonElement(
    MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls) {
  setType(input_type_names::kButton);
  setAttribute(
      html_names::kAriaLabelAttr,
      WTF::AtomicString(GetLocale().QueryString(IDS_AX_MEDIA_DOWNLOAD_BUTTON)));

  SetShadowPseudoId(AtomicString("-internal-media-controls-download-button"));
  SetIsWanted(false);
}

bool MediaControlDownloadButtonElement::ShouldDisplayDownloadButton() const {
  if (!MediaElement().SupportsSave())
    return false;

  // The attribute disables the download button.
  // This is run after `SupportSave()` to guarantee that it is recorded only if
  // it blocks the download button from showing up.
  if (MediaElement().ControlsListInternal()->ShouldHideDownload()) {
    UseCounter::Count(MediaElement().GetDocument(),
                      WebFeature::kHTMLMediaElementControlsListNoDownload);
    return false;
  }

  return true;
}

int MediaControlDownloadButtonElement::GetOverflowStringId() const {
  return IDS_MEDIA_OVERFLOW_MENU_DOWNLOAD;
}

bool MediaControlDownloadButtonElement::HasOverflowButton() const {
  return true;
}

bool MediaControlDownloadButtonElement::IsControlPanelButton() const {
  return true;
}

void MediaControlDownloadButtonElement::Trace(blink::Visitor* visitor) {
  MediaControlInputElement::Trace(visitor);
}

const char* MediaControlDownloadButtonElement::GetNameForHistograms() const {
  return IsOverflowElement() ? "DownloadOverflowButton" : "DownloadButton";
}

void MediaControlDownloadButtonElement::DefaultEventHandler(Event& event) {
  const KURL& url = MediaElement().downloadURL();
  if ((event.type() == event_type_names::kClick ||
       event.type() == event_type_names::kGesturetap) &&
      !(url.IsNull() || url.IsEmpty())) {
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.Download"));
    ResourceRequest request(url);
    request.SetSuggestedFilename(MediaElement().title());
    request.SetRequestContext(mojom::RequestContextType::DOWNLOAD);
    request.SetRequestorOrigin(SecurityOrigin::Create(GetDocument().Url()));
    GetDocument().GetFrame()->Client()->DownloadURL(
        request, network::mojom::RedirectMode::kError);
  }
  MediaControlInputElement::DefaultEventHandler(event);
}

}  // namespace blink
