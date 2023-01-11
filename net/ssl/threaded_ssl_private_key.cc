// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/threaded_ssl_private_key.h"

#include <string>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"

namespace net {

namespace {

void DoCallback(
    const base::WeakPtr<ThreadedSSLPrivateKey>& key,
    SSLPrivateKey::SignCallback callback,
    std::tuple<Error, std::unique_ptr<std::vector<uint8_t>>> result) {
  auto [error, signature] = std::move(result);
  if (!key)
    return;
  std::move(callback).Run(error, *signature);
}

}  // anonymous namespace

class ThreadedSSLPrivateKey::Core
    : public base::RefCountedThreadSafe<ThreadedSSLPrivateKey::Core> {
 public:
  explicit Core(std::unique_ptr<ThreadedSSLPrivateKey::Delegate> delegate)
      : delegate_(std::move(delegate)) {}

  ThreadedSSLPrivateKey::Delegate* delegate() { return delegate_.get(); }

  std::tuple<Error, std::unique_ptr<std::vector<uint8_t>>> Sign(
      uint16_t algorithm,
      base::span<const uint8_t> input) {
    auto signature = std::make_unique<std::vector<uint8_t>>();
    auto error = delegate_->Sign(algorithm, input, signature.get());
    return std::make_tuple(error, std::move(signature));
  }

 private:
  friend class base::RefCountedThreadSafe<Core>;
  ~Core() = default;

  std::unique_ptr<ThreadedSSLPrivateKey::Delegate> delegate_;
};

ThreadedSSLPrivateKey::ThreadedSSLPrivateKey(
    std::unique_ptr<ThreadedSSLPrivateKey::Delegate> delegate,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : core_(base::MakeRefCounted<Core>(std::move(delegate))),
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
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ThreadedSSLPrivateKey::Core::Sign, core_, algorithm,
                     std::vector<uint8_t>(input.begin(), input.end())),
      base::BindOnce(&DoCallback, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

ThreadedSSLPrivateKey::~ThreadedSSLPrivateKey() = default;

}  // namespace net
