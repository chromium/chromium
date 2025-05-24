// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_EVENT_HOLDER_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_EVENT_HOLDER_H_

#import <optional>

#import "base/memory/weak_ptr.h"
#import "base/no_destructor.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_event.h"

// Singleton class holding the latest ChooseFileEvent object. This is a
// singleton because it has the same lifecycle as a JavaScriptFeatures.
class ChooseFileEventHolder {
 public:
  ChooseFileEventHolder(const ChooseFileEventHolder&) = delete;
  ChooseFileEventHolder& operator=(const ChooseFileEventHolder&) = delete;

  static ChooseFileEventHolder* GetInstance();

  // Returns and reset the last ChooseFileEvent.
  std::optional<ChooseFileEvent> ResetLastChooseFileEvent();

  // Set the last ChooseFileEvent.
  void SetLastChooseFileEvent(ChooseFileEvent event);

  // Returns whether `last_choose_file_event_` has a value.
  bool HasLastChooseFileEvent() const;

 private:
  friend class base::NoDestructor<ChooseFileEventHolder>;
  ChooseFileEventHolder();
  ~ChooseFileEventHolder();

  // Latest `ChooseFileEvent` received from JavaScript.
  std::optional<ChooseFileEvent> last_choose_file_event_;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_EVENT_HOLDER_H_
