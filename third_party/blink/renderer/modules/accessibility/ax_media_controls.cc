/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/accessibility/ax_media_controls.h"

#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_time_display_element.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

using blink::WebLocalizedString;
using namespace HTMLNames;

static inline String QueryString(WebLocalizedString::Name name) {
  return Locale::DefaultLocale().QueryString(name);
}

AccessibilityMediaControl::AccessibilityMediaControl(
    LayoutObject* layout_object,
    AXObjectCacheImpl& ax_object_cache)
    : AXLayoutObject(layout_object, ax_object_cache) {}

AXObject* AccessibilityMediaControl::Create(
    LayoutObject* layout_object,
    AXObjectCacheImpl& ax_object_cache) {
  DCHECK(layout_object->GetNode());

  switch (MediaControlElementsHelper::GetMediaControlElementType(
      layout_object->GetNode())) {
    case kMediaSlider:
      return AccessibilityMediaTimeline::Create(layout_object, ax_object_cache);

    case kMediaVolumeSlider:
      return AccessibilityMediaVolumeSlider::Create(layout_object,
                                                    ax_object_cache);

    case kMediaCurrentTimeDisplay:
    case kMediaTimeRemainingDisplay:
      return AccessibilityMediaTimeDisplay::Create(layout_object,
                                                   ax_object_cache);

    case kMediaControlsPanel:
      return AXMediaControlsContainer::Create(layout_object, ax_object_cache);

    case kMediaEnterFullscreenButton:
    case kMediaMuteButton:
    case kMediaPlayButton:
    case kMediaSliderThumb:
    case kMediaShowClosedCaptionsButton:
    case kMediaHideClosedCaptionsButton:
    case kMediaTextTrackList:
    case kMediaUnMuteButton:
    case kMediaPauseButton:
    case kMediaTimelineContainer:
    case kMediaTrackSelectionCheckmark:
    case kMediaVolumeSliderContainer:
    case kMediaVolumeSliderThumb:
    case kMediaExitFullscreenButton:
    case kMediaCastOffButton:
    case kMediaCastOnButton:
    case kMediaOverlayCastOffButton:
    case kMediaOverlayCastOnButton:
    case kMediaOverflowButton:
    case kMediaOverflowList:
    case kMediaDownloadButton:
    case kMediaScrubbingMessage:
    case kMediaEnterPictureInPictureButton:
    case kMediaExitPictureInPictureButton:
    case kMediaDisplayCutoutFullscreenButton:
    case kMediaAnimatedArrowContainer:
      return new AccessibilityMediaControl(layout_object, ax_object_cache);
  }

  NOTREACHED();
  return new AccessibilityMediaControl(layout_object, ax_object_cache);
}

MediaControlElementType AccessibilityMediaControl::ControlType() const {
  if (!GetLayoutObject() || !GetLayoutObject()->GetNode())
    return kMediaTimelineContainer;  // Timeline container is not accessible.

  return MediaControlElementsHelper::GetMediaControlElementType(
      GetLayoutObject()->GetNode());
}

bool AccessibilityMediaControl::InternalSetAccessibilityFocusAction() {
  MediaControlElementsHelper::NotifyMediaControlAccessibleFocus(GetElement());
  return AXLayoutObject::InternalSetAccessibilityFocusAction();
}

bool AccessibilityMediaControl::InternalClearAccessibilityFocusAction() {
  MediaControlElementsHelper::NotifyMediaControlAccessibleBlur(GetElement());
  return AXLayoutObject::InternalClearAccessibilityFocusAction();
}

String AccessibilityMediaControl::TextAlternative(
    bool recursive,
    bool in_aria_labelled_by_traversal,
    AXObjectSet& visited,
    ax::mojom::NameFrom& name_from,
    AXRelatedObjectVector* related_objects,
    NameSources* name_sources) const {
  switch (ControlType()) {
    case kMediaEnterFullscreenButton:
      return QueryString(WebLocalizedString::kAXMediaEnterFullscreenButton);
    case kMediaExitFullscreenButton:
      return QueryString(WebLocalizedString::kAXMediaExitFullscreenButton);
    case kMediaMuteButton:
      return QueryString(WebLocalizedString::kAXMediaMuteButton);
    case kMediaPlayButton:
      return QueryString(WebLocalizedString::kAXMediaPlayButton);
    case kMediaUnMuteButton:
      return QueryString(WebLocalizedString::kAXMediaUnMuteButton);
    case kMediaPauseButton:
      return QueryString(WebLocalizedString::kAXMediaPauseButton);
    case kMediaCurrentTimeDisplay:
      return QueryString(WebLocalizedString::kAXMediaCurrentTimeDisplay);
    case kMediaTimeRemainingDisplay:
      return QueryString(WebLocalizedString::kAXMediaTimeRemainingDisplay);
    case kMediaShowClosedCaptionsButton:
      return QueryString(WebLocalizedString::kAXMediaShowClosedCaptionsButton);
    case kMediaHideClosedCaptionsButton:
      return QueryString(WebLocalizedString::kAXMediaHideClosedCaptionsButton);
    case kMediaCastOffButton:
    case kMediaOverlayCastOffButton:
      return QueryString(WebLocalizedString::kAXMediaCastOffButton);
    case kMediaCastOnButton:
    case kMediaOverlayCastOnButton:
      return QueryString(WebLocalizedString::kAXMediaCastOnButton);
    case kMediaDownloadButton:
      return QueryString(WebLocalizedString::kAXMediaDownloadButton);
    case kMediaOverflowButton:
      return QueryString(WebLocalizedString::kAXMediaOverflowButton);
    case kMediaSliderThumb:
    case kMediaTextTrackList:
    case kMediaTimelineContainer:
    case kMediaTrackSelectionCheckmark:
    case kMediaControlsPanel:
    case kMediaVolumeSliderContainer:
    case kMediaVolumeSliderThumb:
    case kMediaOverflowList:
    case kMediaScrubbingMessage:
    case kMediaAnimatedArrowContainer:
      return QueryString(WebLocalizedString::kAXMediaDefault);
    case kMediaEnterPictureInPictureButton:
      return QueryString(
          WebLocalizedString::kAXMediaEnterPictureInPictureButton);
    case kMediaExitPictureInPictureButton:
      return QueryString(
          WebLocalizedString::kAXMediaExitPictureInPictureButton);
    case kMediaDisplayCutoutFullscreenButton:
      return QueryString(
          WebLocalizedString::kAXMediaDisplayCutoutFullscreenButton);
    case kMediaSlider:
    case kMediaVolumeSlider:
      NOTREACHED();
      return QueryString(WebLocalizedString::kAXMediaDefault);
  }

  NOTREACHED();
  return QueryString(WebLocalizedString::kAXMediaDefault);
}

String AccessibilityMediaControl::Description(
    ax::mojom::NameFrom name_from,
    ax::mojom::DescriptionFrom& description_from,
    AXObjectVector* description_objects) const {
  switch (ControlType()) {
    case kMediaCurrentTimeDisplay:
      return QueryString(WebLocalizedString::kAXMediaCurrentTimeDisplayHelp);
    case kMediaTimeRemainingDisplay:
      return QueryString(WebLocalizedString::kAXMediaTimeRemainingDisplayHelp);
    case kMediaOverflowButton:
      return QueryString(WebLocalizedString::kAXMediaOverflowButtonHelp);
    // The following descriptions are repeats of their respective titles. When
    // read by accessibility, we get the same thing said twice, with no value
    // added. So instead, we just return an empty string.
    case kMediaEnterFullscreenButton:
    case kMediaExitFullscreenButton:
    case kMediaDisplayCutoutFullscreenButton:
    case kMediaMuteButton:
    case kMediaUnMuteButton:
    case kMediaPlayButton:
    case kMediaPauseButton:
    case kMediaShowClosedCaptionsButton:
    case kMediaHideClosedCaptionsButton:
    case kMediaCastOffButton:
    case kMediaOverlayCastOffButton:
    case kMediaCastOnButton:
    case kMediaOverlayCastOnButton:
    case kMediaEnterPictureInPictureButton:
    case kMediaExitPictureInPictureButton:
      return "";
    case kMediaSliderThumb:
    case kMediaTextTrackList:
    case kMediaTimelineContainer:
    case kMediaTrackSelectionCheckmark:
    case kMediaControlsPanel:
    case kMediaVolumeSliderContainer:
    case kMediaVolumeSliderThumb:
    case kMediaOverflowList:
    case kMediaDownloadButton:
    case kMediaScrubbingMessage:
    case kMediaAnimatedArrowContainer:
      return QueryString(WebLocalizedString::kAXMediaDefault);
    case kMediaSlider:
    case kMediaVolumeSlider:
      NOTREACHED();
      return QueryString(WebLocalizedString::kAXMediaDefault);
  }

  NOTREACHED();
  return QueryString(WebLocalizedString::kAXMediaDefault);
}

bool AccessibilityMediaControl::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignored_reasons) const {
  if (!layout_object_ || !layout_object_->Style() ||
      layout_object_->Style()->Visibility() != EVisibility::kVisible ||
      ControlType() == kMediaTimelineContainer)
    return true;

  return AccessibilityIsIgnoredByDefault(ignored_reasons);
}

ax::mojom::Role AccessibilityMediaControl::RoleValue() const {
  switch (ControlType()) {
    case kMediaEnterFullscreenButton:
    case kMediaExitFullscreenButton:
    case kMediaMuteButton:
    case kMediaPlayButton:
    case kMediaUnMuteButton:
    case kMediaPauseButton:
    case kMediaShowClosedCaptionsButton:
    case kMediaHideClosedCaptionsButton:
    case kMediaOverlayCastOffButton:
    case kMediaOverlayCastOnButton:
    case kMediaOverflowButton:
    case kMediaDownloadButton:
    case kMediaCastOnButton:
    case kMediaCastOffButton:
    case kMediaEnterPictureInPictureButton:
    case kMediaExitPictureInPictureButton:
    case kMediaDisplayCutoutFullscreenButton:
      return ax::mojom::Role::kButton;

    case kMediaTimelineContainer:
    case kMediaVolumeSliderContainer:
    case kMediaTextTrackList:
    case kMediaOverflowList:
      return ax::mojom::Role::kGroup;

    case kMediaControlsPanel:
    case kMediaCurrentTimeDisplay:
    case kMediaTimeRemainingDisplay:
    case kMediaSliderThumb:
    case kMediaTrackSelectionCheckmark:
    case kMediaVolumeSliderThumb:
    case kMediaScrubbingMessage:
    case kMediaAnimatedArrowContainer:
      return ax::mojom::Role::kUnknown;

    case kMediaSlider:
    case kMediaVolumeSlider:
      // Not using AccessibilityMediaControl.
      NOTREACHED();
      return ax::mojom::Role::kUnknown;
  }

  NOTREACHED();
  return ax::mojom::Role::kUnknown;
}

//
// AXMediaControlsContainer

AXMediaControlsContainer::AXMediaControlsContainer(
    LayoutObject* layout_object,
    AXObjectCacheImpl& ax_object_cache)
    : AccessibilityMediaControl(layout_object, ax_object_cache) {}

AXObject* AXMediaControlsContainer::Create(LayoutObject* layout_object,
                                           AXObjectCacheImpl& ax_object_cache) {
  return new AXMediaControlsContainer(layout_object, ax_object_cache);
}

String AXMediaControlsContainer::TextAlternative(
    bool recursive,
    bool in_aria_labelled_by_traversal,
    AXObjectSet& visited,
    ax::mojom::NameFrom& name_from,
    AXRelatedObjectVector* related_objects,
    NameSources* name_sources) const {
  return QueryString(IsControllingVideoElement()
                         ? WebLocalizedString::kAXMediaVideoElement
                         : WebLocalizedString::kAXMediaAudioElement);
}

String AXMediaControlsContainer::Description(
    ax::mojom::NameFrom name_from,
    ax::mojom::DescriptionFrom& description_from,
    AXObjectVector* description_objects) const {
  return QueryString(IsControllingVideoElement()
                         ? WebLocalizedString::kAXMediaVideoElementHelp
                         : WebLocalizedString::kAXMediaAudioElementHelp);
}

bool AXMediaControlsContainer::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignored_reasons) const {
  return AccessibilityIsIgnoredByDefault(ignored_reasons);
}

//
// AccessibilityMediaTimeline

static String LocalizedMediaTimeDescription(float /*time*/) {
  // FIXME: To be fixed. See
  // http://trac.webkit.org/browser/trunk/Source/WebCore/platform/LocalizedStrings.cpp#L928
  return String();
}

AccessibilityMediaTimeline::AccessibilityMediaTimeline(
    LayoutObject* layout_object,
    AXObjectCacheImpl& ax_object_cache)
    : AXSlider(layout_object, ax_object_cache) {}

AXObject* AccessibilityMediaTimeline::Create(
    LayoutObject* layout_object,
    AXObjectCacheImpl& ax_object_cache) {
  return new AccessibilityMediaTimeline(layout_object, ax_object_cache);
}

String AccessibilityMediaTimeline::Description(
    ax::mojom::NameFrom name_from,
    ax::mojom::DescriptionFrom& description_from,
    AXObjectVector* description_objects) const {
  return QueryString(IsControllingVideoElement()
                         ? WebLocalizedString::kAXMediaVideoSliderHelp
                         : WebLocalizedString::kAXMediaAudioSliderHelp);
}

//
// AccessibilityMediaVolumeSlider

AccessibilityMediaVolumeSlider::AccessibilityMediaVolumeSlider(
    LayoutObject* layout_object,
    AXObjectCacheImpl& ax_object_cache)
    : AXSlider(layout_object, ax_object_cache) {}

AXObject* AccessibilityMediaVolumeSlider::Create(
    LayoutObject* layout_object,
    AXObjectCacheImpl& ax_object_cache) {
  return new AccessibilityMediaVolumeSlider(layout_object, ax_object_cache);
}

String AccessibilityMediaVolumeSlider::Description(
    ax::mojom::NameFrom name_from,
    ax::mojom::DescriptionFrom& description_from,
    AXObjectVector* description_objects) const {
  return QueryString(WebLocalizedString::kAXMediaVolumeSliderHelp);
}

bool AccessibilityMediaVolumeSlider::InternalSetAccessibilityFocusAction() {
  MediaControlElementsHelper::NotifyMediaControlAccessibleFocus(GetElement());
  return AXSlider::InternalSetAccessibilityFocusAction();
}

bool AccessibilityMediaVolumeSlider::InternalClearAccessibilityFocusAction() {
  MediaControlElementsHelper::NotifyMediaControlAccessibleBlur(GetElement());
  return AXSlider::InternalClearAccessibilityFocusAction();
}

//
// AccessibilityMediaTimeDisplay

AccessibilityMediaTimeDisplay::AccessibilityMediaTimeDisplay(
    LayoutObject* layout_object,
    AXObjectCacheImpl& ax_object_cache)
    : AccessibilityMediaControl(layout_object, ax_object_cache) {}

AXObject* AccessibilityMediaTimeDisplay::Create(
    LayoutObject* layout_object,
    AXObjectCacheImpl& ax_object_cache) {
  return new AccessibilityMediaTimeDisplay(layout_object, ax_object_cache);
}

bool AccessibilityMediaTimeDisplay::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignored_reasons) const {
  if (!layout_object_ || !layout_object_->Style() ||
      layout_object_->Style()->Visibility() != EVisibility::kVisible)
    return true;

  if (!layout_object_->Style()->Width().Value())
    return true;

  return AccessibilityIsIgnoredByDefault(ignored_reasons);
}

String AccessibilityMediaTimeDisplay::TextAlternative(
    bool recursive,
    bool in_aria_labelled_by_traversal,
    AXObjectSet& visited,
    ax::mojom::NameFrom& name_from,
    AXRelatedObjectVector* related_objects,
    NameSources* name_sources) const {
  if (ControlType() == kMediaCurrentTimeDisplay)
    return QueryString(WebLocalizedString::kAXMediaCurrentTimeDisplay);
  return QueryString(WebLocalizedString::kAXMediaTimeRemainingDisplay);
}

String AccessibilityMediaTimeDisplay::StringValue() const {
  if (!layout_object_ || !layout_object_->GetNode())
    return String();

  MediaControlTimeDisplayElement* element =
      static_cast<MediaControlTimeDisplayElement*>(layout_object_->GetNode());
  float time = element->CurrentValue();
  return LocalizedMediaTimeDescription(fabsf(time));
}

}  // namespace blink
