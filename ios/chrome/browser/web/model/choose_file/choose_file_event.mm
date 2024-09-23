// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/choose_file/choose_file_event.h"

#import "ios/web/public/web_state.h"

ChooseFileEvent::ChooseFileEvent(
    bool allow_multiple_files,
    std::vector<std::string> accept_file_extensions,
    std::vector<std::string> accept_mime_types,
    web::WebState* web_state,
    base::Time time)
    : allow_multiple_files{allow_multiple_files},
      accept_file_extensions{std::move(accept_file_extensions)},
      accept_mime_types{std::move(accept_mime_types)},
      web_state{web_state->GetWeakPtr()},
      time{std::move(time)} {}

ChooseFileEvent::ChooseFileEvent(const ChooseFileEvent& event) = default;

ChooseFileEvent::ChooseFileEvent(ChooseFileEvent&& event) = default;

ChooseFileEvent::~ChooseFileEvent() = default;

ChooseFileEvent& ChooseFileEvent::operator=(const ChooseFileEvent& event) =
    default;

ChooseFileEvent& ChooseFileEvent::operator=(ChooseFileEvent&& event) = default;
