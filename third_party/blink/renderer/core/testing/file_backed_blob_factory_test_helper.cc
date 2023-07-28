// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/file_backed_blob_factory_test_helper.h"

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file_backed_blob_factory_dispatcher.h"

namespace blink {

FileBackedBlobFactoryTestHelper::FileBackedBlobFactoryTestHelper(
    ExecutionContext* context)
    : context_(context), receiver_(&factory_) {
  CHECK(context);
  FileBackedBlobFactoryDispatcher::From(*context)
      ->SetFileBackedBlobFactoryForTesting(
          receiver_.BindNewEndpointAndPassDedicatedRemote());
}

FileBackedBlobFactoryTestHelper::~FileBackedBlobFactoryTestHelper() = default;

void FileBackedBlobFactoryTestHelper::FlushForTesting() {
  FileBackedBlobFactoryDispatcher::From(*context_)->FlushForTesting();
}

}  // namespace blink
