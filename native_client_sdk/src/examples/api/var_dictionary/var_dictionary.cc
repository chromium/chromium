// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdio.h>
#include <string>

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_dictionary.h"

#ifdef WIN32
#undef PostMessage
// Allow 'this' in initializer list
#pragma warning(disable : 4355)
#endif

namespace {

static const char kGetCommand[] = "Get";
static const char kSetCommand[] = "Set";
static const char kDeleteCommand[] = "Delete";
static const char kHasKeyCommand[] = "HasKey";
static const char kGetKeysCommand[] = "GetKeys";

pp::Var MakeResult(const char* cmd, const pp::Var& value,
                   const pp::Var& newDictionary) {
  pp::VarDictionary dict;
  dict.Set("cmd", cmd);
  dict.Set("result", value);
  dict.Set("dict", newDictionary);
  return dict;
}

}  // namespace

class VarDictionaryInstance : public pp::Instance {
 public:
  explicit VarDictionaryInstance(PP_Instance instance)
      : pp::Instance(instance) {}

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    // Create the initial dictionary with some basic values.
    dictionary_.Set("key1", "value1");
    dictionary_.Set("foo", true);

    pp::VarArray array;
    array.Set(0, 1);
    array.Set(1, 2);
    array.Set(2, 3.1415);
    array.Set(3, "four");
    dictionary_.Set("array", array);
    PostResult("", pp::Var());
    return true;
  }

 private:
  virtual void HandleMessage(const pp::Var& var_message) {
    if (!var_message.is_dictionary()) {
      fprintf(stderr, "Unexpected message.\n");
      return;
    }

    pp::VarDictionary dict_message(var_message);
    pp::Var var_command = dict_message.Get("cmd");
    if (!var_command.is_string()) {
      fprintf(stderr, "Expect dict item \"command\" to be a string.\n");
      return;
    }

    std::string command = var_command.AsString();
    if (command == kGetCommand) {
      HandleGet(dict_message);
    } else if (command == kSetCommand) {
      HandleSet(dict_message);
    } else if (command == kDeleteCommand) {
      HandleDelete(dict_message);
    } else if (command == kGetKeysCommand) {
      HandleGetKeys(dict_message);
    } else if (command == kHasKeyCommand) {
      HandleHasKey(dict_message);
    }
  }

  void HandleGet(const pp::VarDictionary& dict_message) {
    pp::Var var_key = dict_message.Get("key");
    if (!var_key.is_string()) {
      fprintf(stderr, "HandleGet: Expect dict item \"key\" to be a string.\n");
      return;
    }

    std::string key = var_key.AsString();
    PostResult(kGetCommand, dictionary_.Get(key));
  }

  void HandleSet(const pp::VarDictionary& dict_message) {
    pp::Var var_key = dict_message.Get("key");
    if (!var_key.is_string()) {
      fprintf(stderr, "HandleGet: Expect dict item \"key\" to be a string.\n");
      return;
    }

    pp::Var var_value = dict_message.Get("value");
    std::string key = var_key.AsString();
    PostResult(kSetCommand, dictionary_.Set(key, var_value));
  }

  void HandleDelete(const pp::VarDictionary& dict_message) {
    pp::Var var_key = dict_message.Get("key");
    if (!var_key.is_string()) {
      fprintf(stderr, "HandleGet: Expect dict item \"key\" to be a string.\n");
      return;
    }

    std::string key = var_key.AsString();
    dictionary_.Delete(key);
    PostResult(kDeleteCommand, pp::Var());
  }

  void HandleGetKeys(const pp::VarDictionary& dict_message) {
    PostResult(kGetKeysCommand, dictionary_.GetKeys());
  }

  void HandleHasKey(const pp::VarDictionary& dict_message) {
    pp::Var var_key = dict_message.Get("key");
    if (!var_key.is_string()) {
      fprintf(stderr, "HandleGet: Expect dict item \"key\" to be a string.\n");
      return;
    }

    std::string key = var_key.AsString();
    PostResult(kHasKeyCommand, dictionary_.HasKey(key));
  }

  void PostResult(const char* cmd, const pp::Var& result) {
    PostMessage(MakeResult(cmd, result, dictionary_));
  }

  pp::VarDictionary dictionary_;
};

class VarDictionaryModule : public pp::Module {
 public:
  VarDictionaryModule() : pp::Module() {}
  virtual ~VarDictionaryModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new VarDictionaryInstance(instance);
  }
};

namespace pp {
Module* CreateModule() { return new VarDictionaryModule(); }
}  // namespace pp
