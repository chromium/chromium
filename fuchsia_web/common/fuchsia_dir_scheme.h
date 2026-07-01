// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_COMMON_FUCHSIA_DIR_SCHEME_H_
#define FUCHSIA_WEB_COMMON_FUCHSIA_DIR_SCHEME_H_

#include <string_view>

// URL scheme used to access content directories.
extern const char kFuchsiaDirScheme[];

// Registers kFuchsiaDirScheme as a standard URL scheme.
void RegisterFuchsiaDirScheme();

// Returns true if `content_directory_name` is valid. Valid names are non-empty,
// composed of alphanumeric characters, dots, and dashes, and are not "." or
// "..".
bool IsValidContentDirectoryName(std::string_view content_directory_name);

#endif  // FUCHSIA_WEB_COMMON_FUCHSIA_DIR_SCHEME_H_
