// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/threaded_ssl_private_key.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"

namespace net {

namespace {

void DoCallback(const base::WeakPtr<ThreadedSSLPrivateKey>& key,
                SSLPrivateKey::SignCallback callback,
                std::vector<uint8_t>* signature,
                Error error) {
  if (!key)
    return;
  std::move(callback).Run(error, *signature);
}

}  // anonymous namespace

class ThreadedSSLPrivateKey::Core
    : public base::RefCountedThreadSafe<ThreadedSSLPrivateKey::Core> {
 public:
  Core(std::unique_ptr<ThreadedSSLPrivateKey::Delegate> delegate)
      : delegate_(std::move(delegate)) {}

  ThreadedSSLPrivateKey::Delegate* delegate() { return delegate_.get(); }

  Error Sign(uint16_t algorithm,
             base::span<const uint8_t> input,
             std::vector<uint8_t>* signature) {
    return delegate_->Sign(algorithm, input, signature);
  }

 private:
  friend class base::RefCountedThreadSafe<Core>;
  ~Core() = default;

  std::unique_ptr<ThreadedSSLPrivateKey::Delegate> delegate_;
};

ThreadedSSLPrivateKey::ThreadedSSLPrivateKey(
    std::unique_ptr<ThreadedSSLPrivateKey::Delegate> delegate,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : core_(new Core(std::move(delegate))),
      task_runner_(std::move(task_runner)) {}

std::string ThreadedSSLPrivateKey::GetProviderName() {
  return core_->delegate()->GetProviderName();
}

std::vector<uint16_t> ThreadedSSLPrivateKey::GetAlgorithmPreferences() {
  return core_->delegate()->GetAlgorithmPreferences();
}

void ThreadedSSLPrivateKey::Sign(uint16_t algorithm,
                                 base::span<const uint8_t> input,
                                 SSLPrivateKey::SignCallback callback) {
  std::vector<uint8_t>* signature = new std::vector<uint8_t>;
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&ThreadedSSLPrivateKey::Core::Sign, core_, algorithm,
                     std::vector<uint8_t>(input.begin(), input.end()),
                     base::Unretained(signature)),
      base::BindOnce(&DoCallback, weak_factory_.GetWeakPtr(),
                     std::move(callback), base::Owned(signature)));
}

ThreadedSSLPrivateKey::~ThreadedSSLPrivateKey() = default;

}  // namespace net
