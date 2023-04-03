// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/scoped_fake_ukm_recorder.h"

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/metrics/public/cpp/ukm_recorder_client_interface_registry.h"
#include "services/metrics/public/mojom/ukm_interface.mojom-blink.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
namespace blink {

ScopedFakeUkmRecorder::ScopedFakeUkmRecorder()
    : recorder_(std::make_unique<ukm::TestUkmRecorder>()) {
  Platform::Current()->GetBrowserInterfaceBroker()->SetBinderForTesting(
      ukm::mojom::UkmRecorderFactory::Name_,
      WTF::BindRepeating(
          [](ScopedFakeUkmRecorder* interface,
             mojo::ScopedMessagePipeHandle handle) {
            interface->SetHandle(std::move(handle));
          },
          WTF::Unretained(this)));
}

ScopedFakeUkmRecorder::~ScopedFakeUkmRecorder() {
  Platform::Current()->GetBrowserInterfaceBroker()->SetBinderForTesting(
      ukm::mojom::UkmRecorderFactory::Name_, {});
}

void ScopedFakeUkmRecorder::AddEntry(ukm::mojom::UkmEntryPtr entry) {
  recorder_->AddEntry(std::move(entry));
}
void ScopedFakeUkmRecorder::CreateUkmRecorder(
    mojo::PendingReceiver<ukm::mojom::UkmRecorderInterface> receiver,
    mojo::PendingRemote<ukm::mojom::UkmRecorderClientInterface> client_remote) {
  interface_receiver_ =
      std::make_unique<mojo::Receiver<ukm::mojom::UkmRecorderInterface>>(
          this, mojo::PendingReceiver<ukm::mojom::UkmRecorderInterface>(
                    std::move(receiver)));
  if (client_remote.is_valid()) {
    metrics::UkmRecorderClientInterfaceRegistry::AddClientToCurrentRegistry(
        std::move(client_remote));
  }
}

void ScopedFakeUkmRecorder::UpdateSourceURL(int64_t source_id,
                                            const std::string& url) {
  recorder_->UpdateSourceURL(source_id, GURL(url));
}

void ScopedFakeUkmRecorder::ResetRecorder() {
  recorder_ = std::make_unique<ukm::TestUkmRecorder>();
}

void ScopedFakeUkmRecorder::SetHandle(mojo::ScopedMessagePipeHandle handle) {
  receiver_ = std::make_unique<mojo::Receiver<ukm::mojom::UkmRecorderFactory>>(
      this,
      mojo::PendingReceiver<ukm::mojom::UkmRecorderFactory>(std::move(handle)));
}

}  // namespace blink
