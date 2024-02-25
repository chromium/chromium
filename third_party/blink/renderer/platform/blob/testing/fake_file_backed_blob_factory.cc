// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/blob/testing/fake_file_backed_blob_factory.h"

#include "base/functional/callback_helpers.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/blob/data_element.mojom-blink.h"
#include "third_party/blink/renderer/platform/blob/testing/fake_blob.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_mojo.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

FakeFileBackedBlobFactory::FakeFileBackedBlobFactory() = default;
FakeFileBackedBlobFactory::~FakeFileBackedBlobFactory() = default;

void FakeFileBackedBlobFactory::RegisterBlob(
    mojo::PendingReceiver<mojom::blink::Blob> blob,
    const String& uuid,
    const String& content_type,
    mojom::blink::DataElementFilePtr file) {
  RegisterBlobSync(std::move(blob), uuid, content_type, std::move(file),
                   base::NullCallback());
}

void FakeFileBackedBlobFactory::RegisterBlobSync(
    mojo::PendingReceiver<mojom::blink::Blob> blob,
    const String& uuid,
    const String& content_type,
    mojom::blink::DataElementFilePtr file,
    RegisterBlobSyncCallback callback) {
  registrations.push_back(Registration{uuid, content_type, std::move(file)});

  PostCrossThreadTask(
      *base::ThreadPool::CreateSingleThreadTaskRunner({}), FROM_HERE,
      CrossThreadBindOnce(
          [](const String& uuid,
             mojo::PendingReceiver<mojom::blink::Blob> receiver) {
            mojo::MakeSelfOwnedReceiver(std::make_unique<FakeBlob>(uuid),
                                        std::move(receiver));
          },
          uuid, std::move(blob)));
  if (callback) {
    std::move(callback).Run();
  }
}

}  // namespace blink
