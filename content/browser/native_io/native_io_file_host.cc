// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_io/native_io_file_host.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "content/browser/native_io/native_io_host.h"
#include "content/browser/native_io/native_io_manager.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom.h"

namespace content {

NativeIOFileHost::NativeIOFileHost(
    NativeIOHost* origin_host,
    std::string file_name,
#if BUILDFLAG(IS_MAC)
    bool allow_set_length_ipc,
#endif  // BUILDFLAG(IS_MAC)
    mojo::PendingReceiver<blink::mojom::NativeIOFileHost> file_host_receiver)
    : origin_host_(origin_host),
      file_name_(std::move(file_name)),
#if BUILDFLAG(IS_MAC)
      allow_set_length_ipc_(allow_set_length_ipc),
#endif  // BUILDFLAG(IS_MAC)
      receiver_(this, std::move(file_host_receiver)) {
  // base::Unretained is safe here because this NativeIOFileHost owns
  // |receiver_|. So, the unretained NativeIOFileHost is guaranteed to outlive
  // |receiver_| and the closure that it uses.
  receiver_.set_disconnect_handler(base::BindOnce(
      &NativeIOFileHost::OnReceiverDisconnect, base::Unretained(this)));
}

NativeIOFileHost::~NativeIOFileHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NativeIOFileHost::Close(CloseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(callback).Run();

  // May delete `this`.
  origin_host_->OnFileClose(this, base::PassKey<NativeIOFileHost>());
}

void NativeIOFileHost::OnReceiverDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // May delete `this`.
  origin_host_->OnFileClose(this, base::PassKey<NativeIOFileHost>());
}

}  // namespace content
