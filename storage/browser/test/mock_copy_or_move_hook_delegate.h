// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_MOCK_COPY_OR_MOVE_HOOK_DELEGATE_H_
#define STORAGE_BROWSER_TEST_MOCK_COPY_OR_MOVE_HOOK_DELEGATE_H_

#include "storage/browser/file_system/copy_or_move_hook_delegate.h"

#include "base/functional/callback.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace storage {

class MockCopyOrMoveHookDelegate : public CopyOrMoveHookDelegate {
 public:
  MockCopyOrMoveHookDelegate();

  ~MockCopyOrMoveHookDelegate() override;

  MOCK_METHOD(void,
              OnBeginProcessFile,
              (const FileSystemURL& source_url,
               const FileSystemURL& destination_url,
               StatusCallback callback),
              (override));

  MOCK_METHOD(void,
              OnBeginProcessDirectory,
              (const FileSystemURL& source_url,
               const FileSystemURL& destination_url,
               StatusCallback callback),
              (override));

  MOCK_METHOD(void,
              OnProgress,
              (const FileSystemURL& source_url,
               const FileSystemURL& destination_url,
               int64_t size),
              (override));

  MOCK_METHOD(void,
              OnError,
              (const FileSystemURL& source_url,
               const FileSystemURL& destination_url,
               base::File::Error error,
               ErrorCallback callback),
              (override));

  MOCK_METHOD(void,
              OnEndCopy,
              (const FileSystemURL& source_url,
               const FileSystemURL& destination_url),
              (override));

  MOCK_METHOD(void,
              OnEndMove,
              (const FileSystemURL& source_url,
               const FileSystemURL& destination_url),
              (override));

  MOCK_METHOD(void,
              OnEndRemoveSource,
              (const FileSystemURL& source_url),
              (override));
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_MOCK_COPY_OR_MOVE_HOOK_DELEGATE_H_
