// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/track/automatic_track_selection.h"

#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/html/track/text_track_list.h"
#include "third_party/blink/renderer/platform/language.h"

namespace blink {

class TrackGroup {
  STACK_ALLOCATED();

 public:
  enum GroupKind { kCaptionsAndSubtitles, kDescription, kChapter, kMetadata };

  explicit TrackGroup(GroupKind kind)
      : visible_track(nullptr),
        default_track(nullptr),
        kind(kind),
        has_src_lang(false) {}

  HeapVector<Member<TextTrack>> tracks;
  TextTrack* visible_track;
  TextTrack* default_track;
  GroupKind kind;
  bool has_src_lang;
};

static int TextTrackLanguageSelectionScore(const TextTrack& track) {
  if (track.language().IsEmpty())
    return 0;

  Vector<AtomicString> languages = UserPreferredLanguages();
  wtf_size_t language_match_index =
      IndexOfBestMatchingLanguageInList(track.language(), languages);
  if (language_match_index >= languages.size())
    return 0;

  return languages.size() - language_match_index;
}

static int TextTrackSelectionScore(const TextTrack& track) {
  if (!track.IsVisualKind())
    return 0;

  return TextTrackLanguageSelectionScore(track);
}

AutomaticTrackSelection::AutomaticTrackSelection(
    const Configuration& configuration)
    : configuration_(configuration) {}

const AtomicString& AutomaticTrackSelection::PreferredTrackKind() const {
  if (configuration_.text_track_kind_user_preference ==
      TextTrackKindUserPreference::kSubtitles)
    return TextTrack::SubtitlesKeyword();
  if (configuration_.text_track_kind_user_preference ==
      TextTrackKindUserPreference::kCaptions)
    return TextTrack::CaptionsKeyword();
  return g_null_atom;
}

void AutomaticTrackSelection::PerformAutomaticTextTrackSelection(
    const TrackGroup& group) {
  DCHECK(group.tracks.size());

  // First, find the track in the group that should be enabled (if any).
  HeapVector<Member<TextTrack>> currently_enabled_tracks;
  TextTrack* track_to_enable = nullptr;
  TextTrack* default_track = nullptr;
  TextTrack* preferred_track = nullptr;
  TextTrack* fallback_track = nullptr;
  int highest_track_score = 0;

  for (const auto& text_track : group.tracks) {
    if (configuration_.disable_currently_enabled_tracks &&
        text_track->mode() == TextTrack::ShowingKeyword())
      currently_enabled_tracks.push_back(text_track);

    int track_score = TextTrackSelectionScore(*text_track);

    if (text_track->kind() == PreferredTrackKind())
      track_score += 1;
    if (track_score) {
      // * If the text track kind is subtitles or captions and the user has
      // indicated an interest in having a track with this text track kind, text
      // track language, and text track label enabled, and there is no other
      // text track in the media element's list of text tracks with a text track
      // kind of either subtitles or captions whose text track mode is showing
      //    Let the text track mode be showing.
      if (track_score > highest_track_score) {
        preferred_track = text_track;
        highest_track_score = track_score;
      }
      if (!default_track && text_track->IsDefault())
        default_track = text_track;

      if (!fallback_track)
        fallback_track = text_track;
    } else if (!group.visible_track && !default_track &&
               text_track->IsDefault()) {
      // * If the track element has a default attribute specified, and there is
      // no other text track in the media element's list of text tracks whose
      // text track mode is showing or showing by default
      //    Let the text track mode be showing by default.
      default_track = text_track;
    }
  }

  if (configuration_.text_track_kind_user_preference !=
      TextTrackKindUserPreference::kDefault)
    track_to_enable = preferred_track;

  if (!track_to_enable && default_track)
    track_to_enable = default_track;

  if (!track_to_enable &&
      configuration_.force_enable_subtitle_or_caption_track &&
      group.kind == TrackGroup::kCaptionsAndSubtitles) {
    if (fallback_track)
      track_to_enable = fallback_track;
    else
      track_to_enable = group.tracks[0];
  }

  if (currently_enabled_tracks.size()) {
    for (const auto& text_track : currently_enabled_tracks) {
      if (text_track != track_to_enable)
        text_track->setMode(TextTrack::DisabledKeyword());
    }
  }

  if (track_to_enable)
    track_to_enable->setMode(TextTrack::ShowingKeyword());
}

void AutomaticTrackSelection::EnableDefaultMetadataTextTracks(
    const TrackGroup& group) {
  DCHECK(group.tracks.size());

  // https://html.spec.whatwg.org/C/#honor-user-preferences-for-automatic-text-track-selection

  // 4. If there are any text tracks in the media element's list of text
  // tracks whose text track kind is metadata that correspond to track
  // elements with a default attribute set whose text track mode is set to
  // disabled, then set the text track mode of all such tracks to hidden
  for (auto& text_track : group.tracks) {
    if (text_track->mode() != TextTrack::DisabledKeyword())
      continue;
    if (!text_track->IsDefault())
      continue;
    text_track->setMode(TextTrack::HiddenKeyword());
  }
}

void AutomaticTrackSelection::Perform(TextTrackList& text_tracks) {
  TrackGroup caption_and_subtitle_tracks(TrackGroup::kCaptionsAndSubtitles);
  TrackGroup description_tracks(TrackGroup::kDescription);
  TrackGroup chapter_tracks(TrackGroup::kChapter);
  TrackGroup metadata_tracks(TrackGroup::kMetadata);

  for (wtf_size_t i = 0; i < text_tracks.length(); ++i) {
    TextTrack* text_track = text_tracks.AnonymousIndexedGetter(i);
    if (!text_track)
      continue;

    String kind = text_track->kind();
    TrackGroup* current_group;
    if (kind == TextTrack::SubtitlesKeyword() ||
        kind == TextTrack::CaptionsKeyword()) {
      current_group = &caption_and_subtitle_tracks;
    } else if (kind == TextTrack::DescriptionsKeyword()) {
      current_group = &description_tracks;
    } else if (kind == TextTrack::ChaptersKeyword()) {
      current_group = &chapter_tracks;
    } else {
      DCHECK_EQ(kind, TextTrack::MetadataKeyword());
      current_group = &metadata_tracks;
    }

    if (!current_group->visible_track &&
        text_track->mode() == TextTrack::ShowingKeyword())
      current_group->visible_track = text_track;
    if (!current_group->default_track && text_track->IsDefault())
      current_group->default_track = text_track;

    // Do not add this track to the group if it has already been automatically
    // configured as we only want to perform selection once per track so that
    // adding another track after the initial configuration doesn't reconfigure
    // every track - only those that should be changed by the new addition. For
    // example all metadata tracks are disabled by default, and we don't want a
    // track that has been enabled by script to be disabled automatically when a
    // new metadata track is added later.
    if (text_track->HasBeenConfigured())
      continue;

    if (text_track->language().length())
      current_group->has_src_lang = true;
    current_group->tracks.push_back(text_track);
  }

  if (caption_and_subtitle_tracks.tracks.size())
    PerformAutomaticTextTrackSelection(caption_and_subtitle_tracks);
  if (description_tracks.tracks.size())
    PerformAutomaticTextTrackSelection(description_tracks);
  if (chapter_tracks.tracks.size())
    PerformAutomaticTextTrackSelection(chapter_tracks);
  if (metadata_tracks.tracks.size())
    EnableDefaultMetadataTextTracks(metadata_tracks);
}

}  // namespace blink
