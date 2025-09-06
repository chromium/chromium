// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_CORP_MESSAGING_PLAYGROUND_H_
#define REMOTING_TEST_CORP_MESSAGING_PLAYGROUND_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "remoting/base/internal_headers.h"

namespace network {
class TransitionalURLLoaderFactoryOwner;
}

namespace remoting {

class CorpMessagingClient;
class HttpStatus;

class CorpMessagingPlayground {
 public:
  CorpMessagingPlayground();
  ~CorpMessagingPlayground();

  CorpMessagingPlayground(const CorpMessagingPlayground&) = delete;
  CorpMessagingPlayground& operator=(const CorpMessagingPlayground&) = delete;

  void Start();

 private:
  void OnStreamOpened();
  void OnStreamClosed(base::OnceClosure on_closed, const HttpStatus& status);
  void OnSimpleMessageReceived(const internal::SimpleMessageStruct& message);

  std::unique_ptr<network::TransitionalURLLoaderFactoryOwner>
      url_loader_factory_owner_;
  std::unique_ptr<CorpMessagingClient> client_;
};

}  // namespace remoting

#endif  // REMOTING_TEST_CORP_MESSAGING_PLAYGROUND_H_
