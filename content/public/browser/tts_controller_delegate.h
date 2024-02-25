// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TTS_CONTROLLER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_TTS_CONTROLLER_DELEGATE_H_

#include <memory>
#include <optional>
#include <string>

#include "content/common/content_export.h"

namespace content {

class TtsUtterance;

// Allows embedders to control certain aspects of tts. This is only used on
// ChromeOS.
class CONTENT_EXPORT TtsControllerDelegate {
 public:
  // Used in picking the best Voice for an Utterance.
  struct CONTENT_EXPORT PreferredVoiceId {
    PreferredVoiceId(const std::string& name, const std::string& id);
    PreferredVoiceId();
    ~PreferredVoiceId();

    // Matches against Voice::name.
    std::string name;
    // Matches against Voice::engine_id.
    std::string id;
  };

  struct CONTENT_EXPORT PreferredVoiceIds {
    PreferredVoiceIds();
    PreferredVoiceIds(const PreferredVoiceIds&);
    PreferredVoiceIds& operator=(const PreferredVoiceIds&);
    ~PreferredVoiceIds();

    // The voice ID that matches the language of the utterance, if the user
    // has picked a preferred voice for that language.
    std::optional<PreferredVoiceId> lang_voice_id;

    // The voice ID that matches the language of the system locale, if the user
    // has picked a preferred voice for that locale.
    std::optional<PreferredVoiceId> locale_voice_id;

    // The voice ID that the user has chosen to use when no language code is
    // specified, which can be used to match against any locale.
    std::optional<PreferredVoiceId> any_locale_voice_id;
  };

  // Returns the PreferredVoiceIds for an utterance. PreferredVoiceIds are used
  // in determining which Voice is used for an Utterance.
  virtual std::unique_ptr<PreferredVoiceIds> GetPreferredVoiceIdsForUtterance(
      TtsUtterance* utterance) = 0;

  // Uses the user preferences to update the |rate|, |pitch| and |volume| for
  // a given |utterance|.
  virtual void UpdateUtteranceDefaultsFromPrefs(TtsUtterance* utterance,
                                                double* rate,
                                                double* pitch,
                                                double* volume) = 0;
 protected:
  virtual ~TtsControllerDelegate() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TTS_CONTROLLER_DELEGATE_H_
