// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SAS_INJECTOR_H_
#define REMOTING_HOST_SAS_INJECTOR_H_

#include <memory>

namespace remoting {

// Provides a way to simulate a Secure Attention Sequence (SAS). The default
// sequence is Ctrl+Alt+Delete.
class SasInjector {
 public:
  virtual ~SasInjector() = default;

  // Sends Secure Attention Sequence to the console session.
  virtual bool InjectSas() = 0;

  // Creates an instance of SasInjector if supported by the OS, otherwise
  // returns nullptr.
  static std::unique_ptr<SasInjector> Create();
};

}  // namespace remoting

#endif  // REMOTING_HOST_SAS_INJECTOR_H_
