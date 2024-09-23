// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_fileapi_operation_waiter.h"

#include "base/functional/callback_helpers.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"

namespace content {

TestFileapiOperationWaiter::TestFileapiOperationWaiter() = default;

TestFileapiOperationWaiter::~TestFileapiOperationWaiter() = default;

void TestFileapiOperationWaiter::WaitForOperationToFinish() {
  run_loop_.Run();
}

void TestFileapiOperationWaiter::DidCreate(base::File::Error error_code) {
  run_loop_.Quit();
}

void TestFileapiOperationWaiter::ResultsRetrieved(
    std::vector<filesystem::mojom::DirectoryEntryPtr> entries,
    bool has_more) {
  // blink::mojom::FileSystemManager::ReadDirectory isn't being called by tests
  // right now, so this callback shouldn't be called.
  NOTREACHED_IN_MIGRATION();
}

void TestFileapiOperationWaiter::ErrorOccurred(base::File::Error error_code) {
  run_loop_.Quit();
}

void TestFileapiOperationWaiter::DidWrite(int64_t byte_count, bool complete) {
  if (complete)
    run_loop_.Quit();
}

}  // namespace content
