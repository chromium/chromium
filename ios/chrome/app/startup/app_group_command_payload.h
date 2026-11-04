// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_STARTUP_APP_GROUP_COMMAND_PAYLOAD_H_
#define IOS_CHROME_APP_STARTUP_APP_GROUP_COMMAND_PAYLOAD_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "base/time/time.h"

// App Group command written by a Chrome extension in the shared defaults
// before opening Chrome with the X-Callback `/app-group-command` URL. Not to be
// confused with the ObjC `AppGroupCommand` class used by the extensions to
// write it.
struct AppGroupCommandPayload {
  AppGroupCommandPayload();
  AppGroupCommandPayload(const AppGroupCommandPayload&);
  AppGroupCommandPayload& operator=(const AppGroupCommandPayload&);
  AppGroupCommandPayload(AppGroupCommandPayload&&);
  AppGroupCommandPayload& operator=(AppGroupCommandPayload&&);
  ~AppGroupCommandPayload();

  // Name of the requested command.
  NSString* command = nil;
  // Identifier of the extension that wrote the command. Can be trusted, as
  // only Chrome and its extensions can write to the App Group.
  NSString* caller_app = nil;
  // Time at which the extension wrote the command, if present.
  std::optional<base::Time> command_time;
  // Optional text payload (e.g. the URL to open or the text to search).
  NSString* external_text = nil;
  // Optional binary payload (e.g. the image to search).
  NSData* external_data = nil;
  // Optional index payload.
  NSNumber* index = nil;
  // Whether `command`, `caller_app` and `command_time` are present and
  // `command_time` is within `app_group::kAppGroupCommandTimeout`.
  // Other commands must be discarded, so that an intercepted command can't be
  // replayed later.
  bool is_valid = false;
};

// Reads the App Group command from the shared defaults and removes it, so that
// it is handled only once.
AppGroupCommandPayload ConsumeAppGroupCommandPayload();

#endif  // IOS_CHROME_APP_STARTUP_APP_GROUP_COMMAND_PAYLOAD_H_
