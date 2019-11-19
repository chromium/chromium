// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_INIT_WEB_MAIN_DELEGATE_H_
#define IOS_WEB_PUBLIC_INIT_WEB_MAIN_DELEGATE_H_

namespace web {

// Contains delegate hooks that allow a web/ embedder to customize the basic
// startup and shutdown flow.  This delegate is called very early in startup and
// very late in shutdown, so only minimal code should be run in its
// implementation.  WebMainParts will be a more appropriate place for most
// startup code.
class WebMainDelegate {
 public:
  virtual ~WebMainDelegate() {}

  // Tells the embedder that the absolute basic startup has been done, i.e.
  // it's now safe to create singletons and check the command line.
  virtual void BasicStartupComplete() {}

  // Called right before the process exits.
  // TODO(crbug.com/965895): This may not be used for anything.  Remove if
  // useless.
  virtual void ProcessExiting() {}
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_INIT_WEB_MAIN_DELEGATE_H_
