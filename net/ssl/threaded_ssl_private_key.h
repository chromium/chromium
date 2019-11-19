// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_THREADED_SSL_PRIVATE_KEY_H_
#define NET_SSL_THREADED_SSL_PRIVATE_KEY_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/ssl/ssl_private_key.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {

// An SSLPrivateKey implementation which offloads key operations to a background
// task runner.
class NET_EXPORT ThreadedSSLPrivateKey : public SSLPrivateKey {
 public:
  // Interface for consumers to implement to perform the actual signing
  // operation.
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    // Returns a human-readable name of the provider that backs this
    // SSLPrivateKey, for debugging. If not applicable or available, return the
    // empty string.
    //
    // This method must be efficiently callable on any thread.
    virtual std::string GetProviderName() = 0;

    // Returns the algorithms that are supported by the key in decreasing
    // preference for TLS 1.2 and later. Note that
    // |SSL_SIGN_RSA_PKCS1_MD5_SHA1| is only used by TLS 1.1 and earlier and
    // should not be in this list.
    //
    // This method must be efficiently callable on any thread.
    virtual std::vector<uint16_t> GetAlgorithmPreferences() = 0;

    // Signs an |input| with the specified TLS signing algorithm. |input| is the
    // unhashed message to be signed. On success it returns OK and sets
    // |signature| to the resulting signature. Otherwise it returns a net error
    // code.
    //
    // This method will only be called on the task runner passed to the owning
    // ThreadedSSLPrivateKey.
    virtual Error Sign(uint16_t algorithm,
                       base::span<const uint8_t> input,
                       std::vector<uint8_t>* signature) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  ThreadedSSLPrivateKey(
      std::unique_ptr<Delegate> delegate,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // SSLPrivateKey implementation.
  std::string GetProviderName() override;
  std::vector<uint16_t> GetAlgorithmPreferences() override;
  void Sign(uint16_t algorithm,
            base::span<const uint8_t> input,
            SignCallback callback) override;

 private:
  ~ThreadedSSLPrivateKey() override;
  class Core;

  scoped_refptr<Core> core_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<ThreadedSSLPrivateKey> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ThreadedSSLPrivateKey);
};

}  // namespace net

#endif  // NET_SSL_THREADED_SSL_PRIVATE_KEY_H_
