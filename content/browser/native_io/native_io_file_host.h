// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_FILE_HOST_H_
#define CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_FILE_HOST_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom.h"

namespace content {

class NativeIOHost;

// Implements the NativeIO Web Platform feature for an origin.
//
// NativeIOHost owns an instance of this class for each file opened in a
// renderer. There should be at most one instance of this class for a given
// file name on a NativeIOHost.
//
// This class is not thread-safe, so all access to an instance must happen on
// the same sequence.
class NativeIOFileHost : public blink::mojom::NativeIOFileHost {
 public:
  // `allow_set_length_ipc` gates NativeIOFileHost::SetLength(), which works
  // around a sandboxing limitation on macOS < 10.15. This is plumbed as a flag
  // all the from NativeIOManager to facilitate testing.
  explicit NativeIOFileHost(
      NativeIOHost* origin_host,
      std::string file_name,
#if BUILDFLAG(IS_MAC)
      bool allow_set_length_ipc,
#endif  // BUILDFLAG(IS_MAC)
      mojo::PendingReceiver<blink::mojom::NativeIOFileHost> file_host_receiver);

  NativeIOFileHost(const NativeIOFileHost&) = delete;
  NativeIOFileHost& operator=(const NativeIOFileHost&) = delete;

  ~NativeIOFileHost() override;

  // The name of the file served by this host.
  const std::string& file_name() const { return file_name_; }

  // blink::mojom::NativeIOFileHost:
  void Close(CloseCallback callback) override;
#if BUILDFLAG(IS_MAC)
  void SetLength(const int64_t length,
                 base::File file,
                 SetLengthCallback callback) override;
#endif  // BUILDFLAG(IS_MAC)

 private:
  // Called when the receiver is disconnected.
  void OnReceiverDisconnect();

  SEQUENCE_CHECKER(sequence_checker_);

  // Raw pointer use is safe because NativeIOHost owns this NativeIOFileHost,
  // and therefore is guaranteed to outlive it.
  const raw_ptr<NativeIOHost> origin_host_;

  // The name of the file opened by this host.
  const std::string file_name_;

#if BUILDFLAG(IS_MAC)
  const bool allow_set_length_ipc_;
#endif  // BUILDFLAG(IS_MAC)

  // As long as the receiver is connected, the renderer has an exclusive lock on
  // the file represented by this host.
  mojo::Receiver<blink::mojom::NativeIOFileHost> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_FILE_HOST_H_
