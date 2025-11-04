// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/choose_file/choose_file_event.h"

#import "ios/web/public/web_state.h"

ChooseFileEvent::Builder::Builder() = default;
ChooseFileEvent::Builder::~Builder() = default;

ChooseFileEvent::Builder& ChooseFileEvent::Builder::SetAllowMultipleFiles(
    bool value) {
  allow_multiple_files_ = value;
  return *this;
}

ChooseFileEvent::Builder& ChooseFileEvent::Builder::SetOnlyAllowDirectory(
    bool value) {
  only_allow_directory_ = value;
  return *this;
}

ChooseFileEvent::Builder& ChooseFileEvent::Builder::SetHasSelectedFile(
    bool value) {
  has_selected_file_ = value;
  return *this;
}

ChooseFileEvent::Builder& ChooseFileEvent::Builder::SetAcceptFileExtensions(
    std::vector<std::string> value) {
  accept_file_extensions_ = std::move(value);
  return *this;
}

ChooseFileEvent::Builder& ChooseFileEvent::Builder::SetAcceptMimeTypes(
    std::vector<std::string> value) {
  accept_mime_types_ = std::move(value);
  return *this;
}

ChooseFileEvent::Builder& ChooseFileEvent::Builder::SetWebState(
    web::WebState* value) {
  web_state_ = value;
  return *this;
}

ChooseFileEvent::Builder& ChooseFileEvent::Builder::SetScreenLocation(
    CGPoint value) {
  screen_location_ = value;
  return *this;
}

ChooseFileEvent::Builder& ChooseFileEvent::Builder::SetTime(base::Time value) {
  time_ = value;
  return *this;
}

ChooseFileEvent::Builder& ChooseFileEvent::Builder::SetCapture(
    ChooseFileCaptureType value) {
  capture_ = value;
  return *this;
}

ChooseFileEvent ChooseFileEvent::Builder::Build() {
  CHECK(web_state_);
  return ChooseFileEvent(allow_multiple_files_, only_allow_directory_,
                         has_selected_file_, std::move(accept_file_extensions_),
                         std::move(accept_mime_types_), web_state_,
                         screen_location_, time_, capture_);
}

ChooseFileEvent::ChooseFileEvent(
    bool allow_multiple_files,
    bool only_allow_directory,
    bool has_selected_file,
    std::vector<std::string> accept_file_extensions,
    std::vector<std::string> accept_mime_types,
    web::WebState* web_state,
    CGPoint screen_location,
    base::Time time,
    ChooseFileCaptureType capture)
    : allow_multiple_files{allow_multiple_files},
      only_allow_directory{only_allow_directory},
      has_selected_file{has_selected_file},
      accept_file_extensions{std::move(accept_file_extensions)},
      accept_mime_types{std::move(accept_mime_types)},
      web_state{web_state->GetWeakPtr()},
      screen_location{screen_location},
      time{std::move(time)},
      capture{capture} {}

ChooseFileEvent::ChooseFileEvent(const ChooseFileEvent& event) = default;

ChooseFileEvent::ChooseFileEvent(ChooseFileEvent&& event) = default;

ChooseFileEvent::~ChooseFileEvent() = default;

ChooseFileEvent& ChooseFileEvent::operator=(const ChooseFileEvent& event) =
    default;

ChooseFileEvent& ChooseFileEvent::operator=(ChooseFileEvent&& event) = default;
