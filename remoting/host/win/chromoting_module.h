// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_CHROMOTING_MODULE_H_
#define REMOTING_HOST_WIN_CHROMOTING_MODULE_H_

#include "base/memory/scoped_refptr.h"
#include "base/win/scoped_com_initializer.h"

namespace remoting {

class AutoThreadTaskRunner;

// ChromotingModule runs a MessageLoop allowing Chromium code to post tasks to
// it. Unlike traditional COM servers, ChromotingModule shuts itself down
// immediately once the last COM object is released.
class ChromotingModule {
 public:
  ChromotingModule();

  ChromotingModule(const ChromotingModule&) = delete;
  ChromotingModule& operator=(const ChromotingModule&) = delete;

  ~ChromotingModule();

  // Returns the task runner used by the module. Returns nullptr if the task
  // runner hasn't been registered yet or if the server is shutting down.
  static scoped_refptr<AutoThreadTaskRunner> task_runner();

  // Registers COM classes and runs the main message loop until there are no
  // components using it.
  bool Run();

 private:
  // Used to initialize COM library.
  base::win::ScopedCOMInitializer com_initializer_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_CHROMOTING_MODULE_H_
