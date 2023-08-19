// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/wpt/cwt_stderr_logger.h"

#import "base/files/file.h"
#import "base/files/file_path.h"

CWTStderrLogger* CWTStderrLogger::GetInstance() {
  return base::Singleton<CWTStderrLogger>::get();
}

void CWTStderrLogger::StartRedirectingToFile(const base::FilePath& file_path) {
  base::File destination_file(file_path,
                              base::File::FLAG_OPEN | base::File::FLAG_APPEND);
  DCHECK_EQ(saved_stderr_file_descriptor_, -1);
  saved_stderr_file_descriptor_ = dup(STDERR_FILENO);
  redirection_destination_file_descriptor_ =
      destination_file.TakePlatformFile();
  dup2(redirection_destination_file_descriptor_, STDERR_FILENO);
}

void CWTStderrLogger::StopRedirectingToFile() {
  DCHECK_NE(saved_stderr_file_descriptor_, -1);
  close(redirection_destination_file_descriptor_);
  redirection_destination_file_descriptor_ = -1;

  // Reset stderr to its previous destination, before redirection began.
  dup2(saved_stderr_file_descriptor_, STDERR_FILENO);
  close(saved_stderr_file_descriptor_);
  saved_stderr_file_descriptor_ = -1;
}
