// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>
#include <sstream>
#include <string>

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_dictionary.h"

#include "vpn_provider_helper.h"

#ifdef WIN32
# undef PostMessage
#endif

class VpnProviderInstance : public pp::Instance {
 public:
  explicit VpnProviderInstance(PP_Instance instance)
      : pp::Instance(instance), vpn_provider_helper_(this) {
    vpn_provider_helper_.Init();
  }

  virtual ~VpnProviderInstance() {}

  // Handles messages from Javascript
  virtual void HandleMessage(const pp::Var& message) {
    // Expecting only dictionary messages from JS.
    if (!message.is_dictionary()) {
      PostMessage(
          "NaCl: VpnProviderInstance::HandleMessage: "
          "Unexpected message. Not a dictionary.");
      return;
    }

    // Message type defined by 'cmd' key.
    pp::VarDictionary dict(message);

    std::string command;
    if (!GetStringFromMessage(dict, "cmd", &command)) {
      return;
    }
    if (command == "bind") {
      std::string name, id;
      if (!GetStringFromMessage(dict, "name", &name) ||
          !GetStringFromMessage(dict, "id", &id)) {
        return;
      }
      PostMessage(
          "NaCl: VpnProviderInstance::HandleMessage: "
          "Bind request.");
      vpn_provider_helper_.Bind(name, id);
      return;
    }

    if (command == "connected") {
      PostMessage(
          "NaCl: VpnProviderInstance::HandleMessage: "
          "Connect request.");

      /* This is the place where the developer would establing the VPN
       * connection. The response would usually contain configuration details
       * for the tunnel obtained from the VPN implementation.
       *
       * Currently just signaling that is was executed successfully.
       */

      pp::VarDictionary dict;
      dict.Set("cmd", "setParameters");
      PostMessage(dict);
      return;
    }

    if (command == "disconnected") {
      PostMessage(
          "NaCl: VpnProviderInstance::HandleMessage: "
          "Disconnect request.");

      /* This is the place where the developer would disconnect from the VPN
       * connection.
       */

      return;
    }

    PostMessage(
        "NaCl: VpnProviderInstance::HandleMessage: "
        "Unexpected command.");
  }

 private:
  // Helper function for HandleMessage
  bool GetStringFromMessage(const pp::VarDictionary& dict,
                            const char* key,
                            std::string* value) {
    if (!value)
      return false;

    pp::Var val = dict.Get(key);
    if (val.is_undefined()) {
      std::stringstream ss;
      ss << "NaCl: VpnProviderInstance::HandleMessage: Malformed message. No '"
         << key << "' key.";
      PostMessage(ss.str());
      return false;
    }
    if (!val.is_string()) {
      std::stringstream ss;
      ss << "NaCl: VpnProviderInstance::HandleMessage: Malformed message. ";
      ss << "Type for key '" << key << "' is not string";
      PostMessage(ss.str());
      return false;
    }

    *value = val.AsString();
    return true;
  }

  VpnProviderHelper vpn_provider_helper_;
};

class VpnProviderModule : public pp::Module {
 public:
  VpnProviderModule() : pp::Module() {}
  virtual ~VpnProviderModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new VpnProviderInstance(instance);
  }
};

namespace pp {

Module* CreateModule() {
  return new VpnProviderModule();
}

}  // namespace pp
