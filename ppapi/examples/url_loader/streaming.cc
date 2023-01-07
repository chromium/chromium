// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This example shows how to use the URLLoader in streaming mode (reading to
// memory as data comes over the network). This example uses PostMessage between
// the plugin and the url_loader.html page in this directory to start the load
// and to communicate the result.
//
// The other mode is to stream to a file instead. See stream_to_file.cc

#include <stdint.h>

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"
#include "ppapi/utility/completion_callback_factory.h"

// When compiling natively on Windows, PostMessage can be #define-d to
// something else.
#ifdef PostMessage
#undef PostMessage
#endif

// Buffer size for reading network data.
const int kBufSize = 1024;

class MyInstance : public pp::Instance {
 public:
  explicit MyInstance(PP_Instance instance)
      : pp::Instance(instance) {
    factory_.Initialize(this);
  }
  virtual ~MyInstance() {
    // Make sure to explicitly close the loader. If somebody else is holding a
    // reference to the URLLoader object when this class goes out of scope (so
    // the URLLoader outlives "this"), and you have an outstanding read
    // request, the URLLoader will write into invalid memory.
    loader_.Close();
  }

  // Handler for the page sending us messages.
  virtual void HandleMessage(const pp::Var& message_data);

 private:
  // Called to initiate the request.
  void StartRequest(const std::string& url);

  // Callback for the URLLoader to tell us it finished opening the connection.
  void OnOpenComplete(int32_t result);

  // Starts streaming data.
  void ReadMore();

  // Callback for the URLLoader to tell us when it finished a read.
  void OnReadComplete(int32_t result);

  // Forwards the given string to the page.
  void ReportResponse(const std::string& data);

  // Generates completion callbacks scoped to this class.
  pp::CompletionCallbackFactory<MyInstance> factory_;

  pp::URLLoader loader_;
  pp::URLResponseInfo response_;

  // The buffer used for the current read request. This is filled and then
  // copied into content_ to build up the entire document.
  char buf_[kBufSize];

  // All the content loaded so far.
  std::string content_;
};

void MyInstance::HandleMessage(const pp::Var& message_data) {
  if (message_data.is_string() && message_data.AsString() == "go")
    StartRequest("./fetched_content.html");
}

void MyInstance::StartRequest(const std::string& url) {
  content_.clear();

  pp::URLRequestInfo request(this);
  request.SetURL(url);
  request.SetMethod("GET");

  loader_ = pp::URLLoader(this);
  loader_.Open(request,
               factory_.NewCallback(&MyInstance::OnOpenComplete));
}

void MyInstance::OnOpenComplete(int32_t result) {
  if (result != PP_OK) {
    ReportResponse("URL could not be requested");
    return;
  }

  response_ = loader_.GetResponseInfo();

  // Here you would process the headers. A real program would want to at least
  // check the HTTP code and potentially cancel the request.

  // Start streaming.
  ReadMore();
}

void MyInstance::ReadMore() {
  // Note that you specifically want an "optional" callback here. This will
  // allow Read() to return synchronously, ignoring your completion callback,
  // if data is available. For fast connections and large files, reading as
  // fast as we can will make a large performance difference. However, in the
  // case of a synchronous return, we need to be sure to run the callback we
  // created since the loader won't do anything with it.
  pp::CompletionCallback cc =
      factory_.NewOptionalCallback(&MyInstance::OnReadComplete);
  int32_t result = PP_OK;
  do {
    result = loader_.ReadResponseBody(buf_, kBufSize, cc);
    // Handle streaming data directly. Note that we *don't* want to call
    // OnReadComplete here, since in the case of result > 0 it will schedule
    // another call to this function. If the network is very fast, we could
    // end up with a deeply recursive stack.
    if (result > 0)
      content_.append(buf_, result);
  } while (result > 0);

  if (result != PP_OK_COMPLETIONPENDING) {
    // Either we reached the end of the stream (result == PP_OK) or there was
    // an error. We want OnReadComplete to get called no matter what to handle
    // that case, whether the error is synchronous or asynchronous. If the
    // result code *is* COMPLETIONPENDING, our callback will be called
    // asynchronously.
    cc.Run(result);
  }
}

void MyInstance::OnReadComplete(int32_t result) {
  if (result == PP_OK) {
    // Streaming the file is complete.
    ReportResponse(content_);
  } else if (result > 0) {
    // The URLLoader just filled "result" number of bytes into our buffer.
    // Save them and perform another read.
    content_.append(buf_, result);
    ReadMore();
  } else {
    // A read error occurred.
    ReportResponse("A read error occurred");
  }
}

void MyInstance::ReportResponse(const std::string& data) {
  PostMessage(pp::Var(data));
}

// This object is the global object representing this plugin library as long
// as it is loaded.
class MyModule : public pp::Module {
 public:
  MyModule() : pp::Module() {}
  virtual ~MyModule() {}

  // Override CreateInstance to create your customized Instance object.
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp
