// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_EVENT_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_EVENT_H_

#import <optional>
#import <string>
#import <vector>

#import "base/memory/weak_ptr.h"
#import "base/time/time.h"

namespace web {
class WebState;
}

// Records information about the user's interaction with a file `<input>`.
struct ChooseFileEvent {
  ChooseFileEvent(bool allow_multiple_files,
                  std::vector<std::string> accept_file_extensions,
                  std::vector<std::string> accept_mime_types,
                  web::WebState* web_state,
                  base::Time time = base::Time::Now());
  ChooseFileEvent(const ChooseFileEvent& event);
  ChooseFileEvent(ChooseFileEvent&& event);
  ~ChooseFileEvent();
  ChooseFileEvent& operator=(const ChooseFileEvent& event);
  ChooseFileEvent& operator=(ChooseFileEvent&& event);

  // Whether the input accepts multiple files.
  bool allow_multiple_files;
  // The file extensions that the input accepts.
  std::vector<std::string> accept_file_extensions;
  // The MIME types that the input accepts.
  std::vector<std::string> accept_mime_types;
  // The WebState that triggered this event.
  base::WeakPtr<web::WebState> web_state;
  // The time at which this event occurred.
  base::Time time;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_EVENT_H_
