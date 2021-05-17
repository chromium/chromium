// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SCOPED_URL_FORWARDER_H_
#define REMOTING_HOST_SCOPED_URL_FORWARDER_H_

#include <memory>

namespace remoting {

// A helper class to set up the remote URL forwarder (i.e. making it the default
// browser of the OS) and unset it when it is destroyed.
class ScopedUrlForwarder {
 public:
  virtual ~ScopedUrlForwarder();

  // Sets up the remote URL forwarder and returns the scoped set up.
  // Do not keep multiple ScopedUrlForwarder instances as it will have undefined
  // behavior.
  static std::unique_ptr<ScopedUrlForwarder> Create();

 protected:
  ScopedUrlForwarder();
};

}  // namespace remoting

#endif  // REMOTING_HOST_SCOPED_URL_FORWARDER_H_
