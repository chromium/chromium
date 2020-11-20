// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_io/native_io_file_host.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "content/browser/native_io/native_io_host.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom.h"

namespace content {

namespace {

// Performs the file I/O work in SetLength().
std::pair<bool, base::File> DoSetLength(const int64_t length, base::File file) {
  bool set_length_success = false;
  DCHECK_GE(length, 0) << "The file length should not be negative";
  set_length_success = file.SetLength(length);

  return {set_length_success, std::move(file)};
}

void DidSetLength(blink::mojom::NativeIOFileHost::SetLengthCallback callback,
                  std::pair<bool, base::File> result) {
  std::move(callback).Run(result.first, std::move(result.second));
}

}  // namespace

NativeIOFileHost::NativeIOFileHost(
    mojo::PendingReceiver<blink::mojom::NativeIOFileHost> file_host_receiver,
    NativeIOHost* origin_host,
    std::string file_name)
    : origin_host_(origin_host),
      receiver_(this, std::move(file_host_receiver)),
      file_name_(std::move(file_name)) {
  // base::Unretained is safe here because this NativeIOFileHost owns
  // |receiver_|. So, the unretained NativeIOFileHost is guaranteed to outlive
  // |receiver_| and the closure that it uses.
  receiver_.set_disconnect_handler(base::BindOnce(
      &NativeIOFileHost::OnReceiverDisconnect, base::Unretained(this)));
}

NativeIOFileHost::~NativeIOFileHost() = default;

void NativeIOFileHost::Close(CloseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(callback).Run();
  origin_host_->OnFileClose(this);  // Deletes |this|.
}

void NativeIOFileHost::SetLength(const int64_t length,
                                 base::File file,
                                 SetLengthCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (length < 0) {
    mojo::ReportBadMessage("The file length cannot be negative.");
    std::move(callback).Run(false, std::move(file));
    return;
  }
  // file.IsValid() does not interact with the file system, so we may call it on
  // this thread.
  if (!file.IsValid()) {
    mojo::ReportBadMessage("The file is invalid.");
    std::move(callback).Run(false, std::move(file));
    return;
  }

  // The NativeIO specification says that calling I/O methods on a file
  // concurrently should result in errors. This check is done in the
  // renderer. We don't need to replicate the check in the browser because
  // performing operations concurrently is not a security issue. A misbehaving
  // renderer would merely be exposed to OS-specific behavior.
  //
  // TaskTraits mirror those of the TaskRunner in NativeIOHost, i.e.,
  // base::MayBlock() for file I/O, base::TaskPriority::USER_VISIBLE as some
  // database operations might be blocking, and
  // base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, as blocking shutdown is not
  // appropriate, yet CONTINUE_ON_SHUTDOWN might require more careful analysis.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&DoSetLength, length, std::move(file)),
      base::BindOnce(&DidSetLength, std::move(callback)));
  return;
}

void NativeIOFileHost::OnReceiverDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  origin_host_->OnFileClose(this);  // Deletes |this|.
}

}  // namespace content
