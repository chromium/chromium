// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_VOICE_SEARCH_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_VOICE_SEARCH_PROVIDER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"

@protocol ApplicationCommands;
class AudioSessionController;
class VoiceSearchController;

namespace ios {
class ChromeBrowserState;
}

// VoiceSearchProvider allows embedders to provide functionality related to
// voice search.
class VoiceSearchProvider {
 public:
  VoiceSearchProvider() = default;
  virtual ~VoiceSearchProvider() = default;

  // Returns true if voice search is enabled.  The other methods in this
  // provider must not be called if this method returns false.
  virtual bool IsVoiceSearchEnabled() const;

  // Returns the list of available voice search languages.
  virtual NSArray* GetAvailableLanguages() const;

  // Returns the singleton audio session controller.
  virtual AudioSessionController* GetAudioSessionController() const;

  // Creates a new VoiceSearchController object.
  virtual scoped_refptr<VoiceSearchController> CreateVoiceSearchController(
      ios::ChromeBrowserState* browser_state) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(VoiceSearchProvider);
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_VOICE_SEARCH_PROVIDER_H_
