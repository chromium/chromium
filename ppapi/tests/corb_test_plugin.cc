// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <sstream>

#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/tests/test_utils.h"

// Windows defines 'PostMessage', so we have to undef it.
#ifdef PostMessage
#undef PostMessage
#endif

// This is a simple C++ Pepper plugin that enables Plugin Power Saver tests.
class CorbTestInstance : public pp::Instance {
 public:
  explicit CorbTestInstance(PP_Instance instance) : pp::Instance(instance) {}
  ~CorbTestInstance() override {}

  void HandleMessage(const pp::Var& message_data) override {
    if (!message_data.is_string())
      return;

    std::string msg = message_data.AsString();
    const std::string kRequestUrlMsgPrefix("requestUrl: ");
    if (msg.substr(0, kRequestUrlMsgPrefix.size()) == kRequestUrlMsgPrefix) {
      // This is for NetworkServiceRestartBrowserTest.Plugin test.
      std::string url = msg.substr(kRequestUrlMsgPrefix.size());
      RequestURLAndPostResponseBody(url);
    }
  }

 private:
  void RequestURLAndPostResponseBody(const std::string& url) {
    pp::URLLoader loader(this);

    const PPB_URLLoaderTrusted* url_loader_trusted_interface = nullptr;
    url_loader_trusted_interface = static_cast<const PPB_URLLoaderTrusted*>(
        pp::Module::Get()->GetBrowserInterface(PPB_URLLOADERTRUSTED_INTERFACE));
    url_loader_trusted_interface->GrantUniversalAccess(loader.pp_resource());

    pp::URLRequestInfo request(this);
    request.SetURL(url);
    request.SetAllowCrossOriginRequests(true);

    std::ostringstream msg_builder;
    std::string response_body;
    int result = OpenURLRequest(this->pp_instance(), &loader, request,
                                PP_OPTIONAL, &response_body);
    if (result == PP_OK) {
      msg_builder << "requestUrl: "
                  << "RESPONSE BODY: " << response_body;
    } else {
      msg_builder << "requestUrl: "
                  << "PPAPI ERROR: " << result;
    }

    PostMessage(pp::Var(msg_builder.str()));
  }
};

class CorbTestModule : public pp::Module {
 public:
  CorbTestModule() : pp::Module() {}
  virtual ~CorbTestModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new CorbTestInstance(instance);
  }
};

namespace pp {

Module* CreateModule() {
  return new CorbTestModule();
}

}  // namespace pp
