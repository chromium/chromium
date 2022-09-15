// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This example shows how to use the URLLoader in "stream to file" mode where
// the browser writes incoming data to a file, which you can read out via the
// file I/O APIs.
//
// This example uses PostMessage between the plugin and the url_loader.html
// page in this directory to start the load and to communicate the result.

#include <stdint.h>

#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
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

  // Callback for when the file is completely filled with the download
  void OnStreamComplete(int32_t result);

  void OnOpenFileComplete(int32_t result);
  void OnReadComplete(int32_t result);

  // Forwards the given string to the page.
  void ReportResponse(const std::string& data);

  // Generates completion callbacks scoped to this class.
  pp::CompletionCallbackFactory<MyInstance> factory_;

  pp::URLLoader loader_;
  pp::URLResponseInfo response_;
  pp::FileRef dest_file_;
  pp::FileIO file_io_;

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
  request.SetStreamToFile(true);

  loader_ = pp::URLLoader(this);
  loader_.Open(request,
               factory_.NewCallback(&MyInstance::OnOpenComplete));
}

void MyInstance::OnOpenComplete(int32_t result) {
  if (result != PP_OK) {
    ReportResponse("URL could not be requested");
    return;
  }

  loader_.FinishStreamingToFile(
      factory_.NewCallback(&MyInstance::OnStreamComplete));
  response_ = loader_.GetResponseInfo();
  dest_file_ = response_.GetBodyAsFileRef();
}

void MyInstance::OnStreamComplete(int32_t result) {
  if (result == PP_OK) {
    file_io_ = pp::FileIO(this);
    file_io_.Open(dest_file_, PP_FILEOPENFLAG_READ,
        factory_.NewCallback(&MyInstance::OnOpenFileComplete));
  } else {
    ReportResponse("Could not stream to file");
  }
}

void MyInstance::OnOpenFileComplete(int32_t result) {
  if (result == PP_OK) {
    // Note we only read the first 1024 bytes from the file in this example
    // to keep things simple. Please see a file I/O example for more details
    // on reading files.
    file_io_.Read(0, buf_, kBufSize,
        factory_.NewCallback(&MyInstance::OnReadComplete));
  } else {
    ReportResponse("Could not open file");
  }
}

void MyInstance::OnReadComplete(int32_t result) {
  if (result >= 0) {
    content_.append(buf_, result);
    ReportResponse(buf_);
  } else {
    ReportResponse("Could not read file");
  }

  // Release everything.
  loader_ = pp::URLLoader();
  response_ = pp::URLResponseInfo();
  dest_file_ = pp::FileRef();
  file_io_ = pp::FileIO();
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
