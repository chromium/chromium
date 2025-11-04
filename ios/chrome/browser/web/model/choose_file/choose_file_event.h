// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_EVENT_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_EVENT_H_

#import <optional>
#import <string>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/time/time.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_util.h"

namespace web {
class WebState;
}

// Records information about the user's interaction with a file `<input>`.
struct ChooseFileEvent {
 public:
  class Builder {
   public:
    Builder();
    ~Builder();

    Builder& SetAllowMultipleFiles(bool value);
    Builder& SetOnlyAllowDirectory(bool value);
    Builder& SetHasSelectedFile(bool value);
    Builder& SetAcceptFileExtensions(std::vector<std::string> value);
    Builder& SetAcceptMimeTypes(std::vector<std::string> value);
    Builder& SetWebState(web::WebState* value);
    Builder& SetScreenLocation(CGPoint value);
    Builder& SetTime(base::Time value);
    Builder& SetCapture(ChooseFileCaptureType value);

    ChooseFileEvent Build();

   private:
    bool allow_multiple_files_ = false;
    bool only_allow_directory_ = false;
    bool has_selected_file_ = false;
    std::vector<std::string> accept_file_extensions_;
    std::vector<std::string> accept_mime_types_;
    raw_ptr<web::WebState> web_state_ = nullptr;
    CGPoint screen_location_{};
    base::Time time_ = base::Time::Now();
    ChooseFileCaptureType capture_ = ChooseFileCaptureType::kNone;
  };

  ChooseFileEvent(const ChooseFileEvent& event);
  ChooseFileEvent(ChooseFileEvent&& event);
  ~ChooseFileEvent();
  ChooseFileEvent& operator=(const ChooseFileEvent& event);
  ChooseFileEvent& operator=(ChooseFileEvent&& event);

  // Whether the input accepts multiple files.
  bool allow_multiple_files;
  // Whether the input only accepts a directory.
  bool only_allow_directory;
  // Whether the input already has selected file.
  bool has_selected_file;
  // The file extensions that the input accepts.
  std::vector<std::string> accept_file_extensions;
  // The MIME types that the input accepts.
  std::vector<std::string> accept_mime_types;
  // The WebState that triggered this event.
  base::WeakPtr<web::WebState> web_state;
  // The location of the event in the screen.
  CGPoint screen_location;
  // The time at which this event occurred.
  base::Time time;
  // The capture setting of the input.
  ChooseFileCaptureType capture;

 private:
  ChooseFileEvent(bool allow_multiple_files,
                  bool only_allow_directory,
                  bool has_selected_file,
                  std::vector<std::string> accept_file_extensions,
                  std::vector<std::string> accept_mime_types,
                  web::WebState* web_state,
                  CGPoint screen_location,
                  base::Time time,
                  ChooseFileCaptureType capture);
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_EVENT_H_
