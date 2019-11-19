// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_VOICE_SEARCH_CONTROLLER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_VOICE_SEARCH_CONTROLLER_H_

#include "base/memory/ref_counted.h"

@protocol LoadQueryCommands;
@class UIViewController;

namespace web {
class WebState;
}

// TODO(crbug.com/607204): Convert to Objective-C class.
class VoiceSearchController
    : public base::RefCountedThreadSafe<VoiceSearchController> {
 public:
  VoiceSearchController();

  // Sets the dispatcher for this object.
  virtual void SetDispatcher(id<LoadQueryCommands> dispatcher);

  // Preloads views and view controllers needed for the voice search UI.
  virtual void PrepareToAppear();

  // Starts recognizing and recording process. Will call the delegate method
  // upon completion if the recognition succeeds.
  // |presenting_view_controller| is the UIViewController from which to present
  // the Voice Search input UI.
  virtual void StartRecognition(UIViewController* presenting_view_controller,
                                web::WebState* current_web_state);

  // Whether or not the Text To Speech user preference is enabled.
  virtual bool IsTextToSpeechEnabled();

  // Returns whether Text To Speech is supported for the currently selected
  // Voice Search language. If a Voice Search language has not been specified,
  // returns whether Text To Speech is supported for the default Voice Search
  // language.
  virtual bool IsTextToSpeechSupported();

  // Returns |true| if the voice search UI is visible.
  virtual bool IsVisible();

  // Returns |true| if text-to-speech audio is currently playing.
  virtual bool IsPlayingAudio();

  // Dismisses the mic permissions help UI, if shown.
  virtual void DismissMicPermissionsHelp();

 protected:
  virtual ~VoiceSearchController();

 private:
  friend class base::RefCountedThreadSafe<VoiceSearchController>;

  DISALLOW_COPY_AND_ASSIGN(VoiceSearchController);
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_VOICE_SEARCH_CONTROLLER_H_
