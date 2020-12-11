// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_FILE_HOST_H_
#define CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_FILE_HOST_H_

#include "base/sequence_checker.h"
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
  explicit NativeIOFileHost(
      mojo::PendingReceiver<blink::mojom::NativeIOFileHost> file_host_receiver,
      NativeIOHost* origin_host,
      std::string file_name);

  NativeIOFileHost(const NativeIOFileHost&) = delete;
  NativeIOFileHost& operator=(const NativeIOFileHost&) = delete;

  ~NativeIOFileHost() override;

  // The name of the file served by this host.
  const std::string& file_name() const { return file_name_; }

  // blink::mojom::NativeIOFileHost:
  void Close(CloseCallback callback) override;

  // blink::mojom::NativeIOFileHost:
  void SetLength(const int64_t length,
                 base::File file,
                 SetLengthCallback callback) override;

 private:
  // Called when the receiver is disconnected.
  void OnReceiverDisconnect();

  // Raw pointer use is safe because NativeIOHost owns this NativeIOFileHost,
  // and therefore is guaranteed to outlive it.
  NativeIOHost* const origin_host_;

  // As long as the receiver is connected, the renderer has an exclusive lock on
  // the file represented by this host.
  mojo::Receiver<blink::mojom::NativeIOFileHost> receiver_;

  // The name of the file opened by this host.
  const std::string file_name_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_FILE_HOST_H_
