// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_WPT_CWT_STDERR_LOGGER_H_
#define IOS_CHROME_TEST_WPT_CWT_STDERR_LOGGER_H_

#include "base/memory/singleton.h"

namespace base {
class FilePath;
}

// This class manages redirecting stderr output to a file. Redirection can be
// started and stopped multiple times, but there must be a call to
// StopRedirectingToFile() between any two calls to StartRedirectingToFile().
class CWTStderrLogger {
 public:
  // Returns the singleton instance of this class.
  static CWTStderrLogger* GetInstance();

  CWTStderrLogger(const CWTStderrLogger&) = delete;
  CWTStderrLogger& operator=(const CWTStderrLogger&) = delete;

  // Starts redirecting stderr output to the file with the given path. This
  // file must already exist. Any existing content will not be overwritten;
  // instead, new content is appended to the file..
  void StartRedirectingToFile(const base::FilePath& file_path);

  // Stops redirecting stderr output to a file.
  void StopRedirectingToFile();

 private:
  friend struct base::DefaultSingletonTraits<CWTStderrLogger>;

  CWTStderrLogger() = default;

  // When redirection is active, this saves a copy of the old stderr file
  // descriptor, which is restored as the destination of stderr after
  // redirection is stopped.
  int saved_stderr_file_descriptor_ = -1;

  // While redirection is active, this stores the file descriptor for the
  // destination file.
  int redirection_destination_file_descriptor_ = -1;
};

#endif  // IOS_CHROME_TEST_WPT_CWT_STDERR_LOGGER_H_
