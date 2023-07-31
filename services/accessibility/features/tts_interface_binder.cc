// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/tts_interface_binder.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "services/accessibility/public/mojom/tts.mojom.h"

namespace ax {

TtsInterfaceBinder::TtsInterfaceBinder(
    base::WeakPtr<mojom::AccessibilityServiceClient> ax_service_client,
    scoped_refptr<base::SequencedTaskRunner> main_runner)
    : ax_service_client_(ax_service_client), main_runner_(main_runner) {}

TtsInterfaceBinder::~TtsInterfaceBinder() = default;

bool TtsInterfaceBinder::MatchesInterface(const std::string& interface_name) {
  return interface_name == "ax.mojom.Tts";
}

void TtsInterfaceBinder::BindReceiver(
    mojo::GenericPendingReceiver tts_receiver) {
  CHECK(main_runner_);
  auto receiver = tts_receiver.As<ax::mojom::Tts>();
  // This might be called on any thread because it's initiated by Mojom.
  // Do the actual binding on the service main thread, where the
  // AccessibilityServiceClient lives.
  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<mojom::AccessibilityServiceClient> ax_service_client,
             mojo::PendingReceiver<ax::mojom::Tts> receiver) {
            ax_service_client->BindTts(std::move(receiver));
          },
          ax_service_client_, std::move(receiver)));
}

}  // namespace ax
