// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_CORP_MESSAGING_PLAYGROUND_H_
#define REMOTING_TEST_CORP_MESSAGING_PLAYGROUND_H_

#include <memory>

namespace network {
class TransitionalURLLoaderFactoryOwner;
}

namespace remoting {

class CorpMessagingPlayground {
 public:
  CorpMessagingPlayground();
  ~CorpMessagingPlayground();

  CorpMessagingPlayground(const CorpMessagingPlayground&) = delete;
  CorpMessagingPlayground& operator=(const CorpMessagingPlayground&) = delete;

  void Start();

 private:
  std::unique_ptr<network::TransitionalURLLoaderFactoryOwner>
      url_loader_factory_owner_;
};

}  // namespace remoting

#endif  // REMOTING_TEST_CORP_MESSAGING_PLAYGROUND_H_
