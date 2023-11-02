// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This example demonstrates how to load content of the page into NaCl module.

#include <cstdio>
#include <string>
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/var.h"

#include "url_loader_handler.h"

// These are the method names as JavaScript sees them.
namespace {
const char* const kLoadUrlMethodId = "getUrl";
static const char kMessageArgumentSeparator = ':';
}  // namespace

class URLLoaderInstance : public pp::Instance {
 public:
  explicit URLLoaderInstance(PP_Instance instance) : pp::Instance(instance) {}
  virtual ~URLLoaderInstance() {}

  // Called by the browser to handle the postMessage() call in Javascript.
  // The message in this case is expected to contain the string 'getUrl'
  // followed by a ':' separator, then the URL to fetch.  If a valid message
  // of the form 'getUrl:URL' is received, then start up an asynchronous
  // download of URL.  In the event that errors occur, this method posts an
  // error string back to the browser.
  virtual void HandleMessage(const pp::Var& var_message);
};

void URLLoaderInstance::HandleMessage(const pp::Var& var_message) {
  if (!var_message.is_string()) {
    return;
  }
  std::string message = var_message.AsString();
  if (message.find(kLoadUrlMethodId) == 0) {
    // The argument to getUrl is everything after the first ':'.
    size_t sep_pos = message.find_first_of(kMessageArgumentSeparator);
    if (sep_pos != std::string::npos) {
      std::string url = message.substr(sep_pos + 1);
      printf("URLLoaderInstance::HandleMessage('%s', '%s')\n",
             message.c_str(),
             url.c_str());
      fflush(stdout);
      URLLoaderHandler* handler = URLLoaderHandler::Create(this, url);
      if (handler != NULL) {
        // Starts asynchronous download. When download is finished or when an
        // error occurs, |handler| posts the results back to the browser
        // vis PostMessage and self-destroys.
        handler->Start();
      }
    }
  }
}

class URLLoaderModule : public pp::Module {
 public:
  URLLoaderModule() : pp::Module() {}
  virtual ~URLLoaderModule() {}

  // Create and return a URLLoaderInstance object.
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new URLLoaderInstance(instance);
  }
};

namespace pp {
Module* CreateModule() { return new URLLoaderModule(); }
}  // namespace pp
