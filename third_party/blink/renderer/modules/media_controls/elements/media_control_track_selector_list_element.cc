// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_track_selector_list_element.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/track/audio_track_list.h"
#include "third_party/blink/renderer/core/html/track/track_list_base.h"
#include "third_party/blink/renderer/core/html/track/video_track_list.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "ui/strings/grit/ax_strings.h"

namespace blink {
namespace {

template <typename TrackType>
String GetLanguage(TrackType* track, Locale& locale) {
  return track->language();
}
template <typename TrackType>
String GetLabel(TrackType* track, Locale& locale) {
  return track->label();
}
template <typename TrackType>
String GetKind(TrackType* track, Locale& locale) {
  return track->kind();
}
template <typename TrackType>
String GetID(TrackType* track, Locale& locale) {
  return locale.QueryString(IDS_MEDIA_TRACK_IDENTIFIER_VERBOSE_TITLE_NUMBERED,
                            locale.ConvertToLocalizedNumber(track->id()));
}

// Applies `pref`:(TrackType*, Locale&)->String to each track in `tracks`
template <typename TrackType, typename Fn>
std::vector<String> GetPreferenceList(const TrackListBase<TrackType>& tracks,
                                      Locale& locale,
                                      Fn pref) {
  std::vector<String> extracted;
  for (uint32_t i = 0; i < tracks.length(); i++) {
    auto entry = pref(tracks.AnonymousIndexedGetter(i), locale);
    if (entry != "") {
      extracted.push_back(entry);
    }
  }
  return extracted;
}

// Given an ordered list of closures of type (TrackType*, Locale&)->String,
// this function will apply each closure to the list of tracks and return the
// first result where all the results are non-empty and unique. The last closure
// must return non-empty, unique strings for each track, so it's ideal to use
// some identification number, stringified.
template <typename TrackType, typename Fn, typename Fb, typename... Rest>
std::vector<String> GetPreferenceList(const TrackListBase<TrackType>& tracks,
                                      Locale& locale,
                                      Fn pref,
                                      Fb fallback,
                                      Rest... rest) {
  std::vector<String> extracted =
      GetPreferenceList<TrackType>(tracks, locale, pref);
  auto new_end = std::unique(extracted.begin(), extracted.end());
  auto new_length = std::distance(extracted.begin(), new_end);
  if (new_length >= 0 && static_cast<size_t>(new_length) == tracks.length()) {
    return extracted;
  }
  return GetPreferenceList(tracks, locale, fallback, rest...);
}

const QualifiedName& SelectedTrackIdAttr() {
  // Save the track id in an attribute.
  DEFINE_STATIC_LOCAL(QualifiedName, selected_track_id,
                      (AtomicString("data-selected-track-id")));
  return selected_track_id;
}

}  // namespace

MediaControlTrackSelectorListElement::MediaControlTrackSelectorListElement(
    MediaControlsImpl& media_controls,
    bool is_video)
    : MediaControlPopupMenuElement(media_controls), is_video_(is_video) {
  setAttribute(html_names::kRoleAttr, AtomicString("menu"));
  setAttribute(html_names::kAriaLabelAttr,
               AtomicString(GetLocale().QueryString(
                   IDS_MEDIA_OVERFLOW_MENU_TRACK_SELECTION_SUBMENU_TITLE)));
  if (is_video) {
    SetShadowPseudoId(
        AtomicString("-internal-media-controls-video-track-selection-list"));
  } else {
    SetShadowPseudoId(
        AtomicString("-internal-media-controls-audio-track-selection-list"));
  }
}

void MediaControlTrackSelectorListElement::SetIsWanted(bool wanted) {
  if (wanted) {
    RemoveChildren();
    RepopulateTrackList();
  }
  if (!wanted && !GetMediaControls().OverflowMenuIsWanted()) {
    GetMediaControls().CloseOverflowMenu();
  }
  MediaControlPopupMenuElement::SetIsWanted(wanted);
}

void MediaControlTrackSelectorListElement::Trace(Visitor* visitor) const {
  MediaControlPopupMenuElement::Trace(visitor);
}

void MediaControlTrackSelectorListElement::DefaultEventHandler(Event& event) {
  if (event.type() == event_type_names::kClick) {
    // This handles the back button click. Clicking on a menu item triggers
    // the change event instead.
    GetMediaControls().ToggleOverflowMenu();
    event.SetDefaultHandled();
  } else if (event.type() == event_type_names::kChange) {
    // Identify which input element was selected and select the corresponding
    // track.
    Node* target = event.target()->ToNode();
    if (!target || !target->IsElementNode()) {
      return;
    }

    auto i = To<Element>(target)->GetIntegralAttribute(SelectedTrackIdAttr());
    if (is_video_) {
      MediaElement().videoTracks().AnonymousIndexedGetter(i)->setSelected(
          true, TrackBase::ChangeSource::kUser);
    } else {
      MediaElement().audioTracks().AnonymousIndexedGetter(i)->setEnabled(
          true, TrackBase::ChangeSource::kUser);
    }

    // Close the list.
    event.SetDefaultHandled();
    SetIsWanted(false);
  }
  MediaControlPopupMenuElement::DefaultEventHandler(event);
}

void MediaControlTrackSelectorListElement::RepopulateTrackList() {
  auto* header_item = MakeGarbageCollected<HTMLLabelElement>(GetDocument());
  header_item->SetShadowPseudoId(
      AtomicString("-internal-media-controls-track-selection-list-header"));
  header_item->ParserAppendChild(
      Text::Create(GetDocument(),
                   GetLocale().QueryString(
                       IDS_MEDIA_OVERFLOW_MENU_TRACK_SELECTION_SUBMENU_TITLE)));
  header_item->setAttribute(html_names::kRoleAttr, AtomicString("button"));
  header_item->setAttribute(html_names::kAriaLabelAttr,
                            AtomicString(GetLocale().QueryString(
                                IDS_AX_MEDIA_BACK_TO_OPTIONS_BUTTON)));
  header_item->setTabIndex(0);
  ParserAppendChild(header_item);

  std::vector<String> labels;
  if (is_video_) {
    labels = GetPreferenceList<VideoTrack>(
        MediaElement().videoTracks(), GetLocale(), &GetLabel<VideoTrack>,
        &GetKind<VideoTrack>, &GetID<VideoTrack>);
  } else {
    labels = GetPreferenceList<AudioTrack>(
        MediaElement().audioTracks(), GetLocale(), &GetLabel<AudioTrack>,
        &GetLanguage<AudioTrack>, &GetKind<AudioTrack>, &GetID<AudioTrack>);
  }

  int index = 0;
  for (const String& label : labels) {
    bool selected = is_video_ ? MediaElement()
                                    .videoTracks()
                                    .AnonymousIndexedGetter(index)
                                    ->selected()
                              : MediaElement()
                                    .audioTracks()
                                    .AnonymousIndexedGetter(index)
                                    ->enabled();
    ParserAppendChild(CreateListItem(label, selected, index));
    index++;
  }
}

Element* MediaControlTrackSelectorListElement::CreateListItem(
    const String& content,
    bool is_checked,
    int index) {
  auto* entry = MakeGarbageCollected<HTMLLabelElement>(GetDocument());
  entry->SetShadowPseudoId(
      AtomicString("-internal-media-controls-track-selection-list-item"));

  auto* selector = MakeGarbageCollected<HTMLInputElement>(GetDocument());
  selector->SetShadowPseudoId(
      AtomicString("-internal-media-controls-track-selection-list-item-input"));
  selector->setAttribute(html_names::kAriaHiddenAttr, keywords::kTrue);
  selector->setType(input_type_names::kCheckbox);
  selector->SetIntegralAttribute(SelectedTrackIdAttr(), index);

  selector->SetChecked(is_checked);

  auto* label = MakeGarbageCollected<HTMLSpanElement>(GetDocument());
  label->setInnerText(content);
  label->setAttribute(html_names::kAriaHiddenAttr, keywords::kTrue);

  selector->setTabIndex(-1);
  entry->setTabIndex(0);
  entry->setAttribute(html_names::kAriaLabelAttr, AtomicString(content));
  entry->ParserAppendChild(label);
  entry->ParserAppendChild(selector);
  return entry;
}

}  // namespace blink
