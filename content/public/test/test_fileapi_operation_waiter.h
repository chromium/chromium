// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_FILEAPI_OPERATION_WAITER_H_
#define CONTENT_PUBLIC_TEST_TEST_FILEAPI_OPERATION_WAITER_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "storage/browser/file_system/file_observers.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom.h"

namespace content {

// Installs a temporary storage::FileUpdateObserver for use in browser tests
// that need to wait for a specific fileapi operation to complete.
class TestFileapiOperationWaiter
    : public blink::mojom::FileSystemOperationListener {
 public:
  TestFileapiOperationWaiter();
  ~TestFileapiOperationWaiter() override;

  void WaitForOperationToFinish();

  // Callback for passing into blink::mojom::FileSystem::Create.
  void DidCreate(base::File::Error error_code);

  // blink::mojom::FileSystemOperationListener
  void ResultsRetrieved(
      std::vector<filesystem::mojom::DirectoryEntryPtr> entries,
      bool has_more) override;
  void ErrorOccurred(base::File::Error error_code) override;
  void DidWrite(int64_t byte_count, bool complete) override;

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestFileapiOperationWaiter);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_FILEAPI_OPERATION_WAITER_H_
