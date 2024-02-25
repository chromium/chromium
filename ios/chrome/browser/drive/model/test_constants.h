// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_TEST_CONSTANTS_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_TEST_CONSTANTS_H_

// Command line switch to configure the behavior of `TestDriveFileUploader`
// during EG tests.
extern const char kTestDriveFileUploaderCommandLineSwitch[];
// Values associated with command line switch
// `kTestDriveFileUploaderCommandLineSwitch`.
extern const char kTestDriveFileUploaderCommandLineSwitchSucceed[];
extern const char kTestDriveFileUploaderCommandLineSwitchFailAndThenSucceed[];

// Possible behaviors of `TestDriveFileUploader`.
enum class TestDriveFileUploaderBehavior {
  // The `TestDriveFileUploader` should return a success result.
  kSucceed,
  // The `TestDriveFileUploader` should return an error result in the first
  // attempt, and then a success result in the second attempt.
  kFailAndThenSucceed,
};

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_TEST_CONSTANTS_H_
