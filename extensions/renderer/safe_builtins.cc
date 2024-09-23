// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/safe_builtins.h"

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/v8_helpers.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-extension.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive-object.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-template.h"

namespace extensions {

namespace {

const char kClassName[] = "extensions::SafeBuiltins";

// Documentation for makeCallback in the JavaScript, out here to reduce the
// (very small) amount of effort that the v8 parser needs to do:
//
// Returns a new object with every function on |obj| configured to call()
// itself with the given arguments.
// E.g. given
//    var result = makeCallable(Function.prototype)
// |result| will be a object including 'bind' such that
//    result.bind(foo, 1, 2, 3);
// is equivalent to Function.prototype.bind.call(foo, 1, 2, 3), and so on.
// This is a convenient way to save functions that user scripts may clobber.
const char kScript[] =
    "(function() {\n"
    "'use strict';\n"
    "native function Apply();\n"
    "native function Save();\n"
    "\n"
    "// Used in the callback implementation, could potentially be clobbered.\n"
    "function makeCallable(obj, target, isStatic, propertyNames) {\n"
    "  propertyNames.forEach(function(propertyName) {\n"
    "    var property = obj[propertyName];\n"
    "    target[propertyName] = function() {\n"
    "      var recv = obj;\n"
    "      var firstArgIndex = 0;\n"
    "      if (!isStatic) {\n"
    "        if (arguments.length == 0)\n"
    "          throw 'There must be at least one argument, the receiver';\n"
    "        recv = arguments[0];\n"
    "        firstArgIndex = 1;\n"
    "      }\n"
    "      return Apply(\n"
    "          property, recv, arguments, firstArgIndex, arguments.length);\n"
    "    };\n"
    "  });\n"
    "}\n"
    "\n"
    "function saveBuiltin(builtin, protoPropertyNames, staticPropertyNames) {\n"
    "  var safe = function() {\n"
    "    throw 'Safe objects cannot be called nor constructed. ' +\n"
    "          'Use $Foo.self() or new $Foo.self() instead.';\n"
    "  };\n"
    "  safe.self = builtin;\n"
    "  makeCallable(builtin.prototype, safe, false, protoPropertyNames);\n"
    "  if (staticPropertyNames)\n"
    "    makeCallable(builtin, safe, true, staticPropertyNames);\n"
    "  Save(builtin.name, safe);\n"
    "}\n"
    "\n"
    "// Save only what is needed by the extension modules.\n"
    "saveBuiltin(Object,\n"
    "            ['hasOwnProperty'],\n"
    "            ['create', 'defineProperty', 'freeze',\n"
    "             'getOwnPropertyDescriptor', 'getPrototypeOf', 'keys',\n"
    "             'assign', 'setPrototypeOf']);\n"
    "saveBuiltin(Function,\n"
    "            ['apply', 'bind', 'call']);\n"
    "saveBuiltin(Array,\n"
    "            ['concat', 'forEach', 'indexOf', 'join', 'push', 'slice',\n"
    "             'splice', 'map', 'filter', 'shift', 'unshift', 'pop',\n"
    "             'reverse'],\n"
    "            ['from', 'isArray']);\n"
    "saveBuiltin(String,\n"
    "            ['indexOf', 'slice', 'split', 'substr', 'toLowerCase',\n"
    "             'toUpperCase', 'replace']);\n"
    "// Use exec rather than test to defend against clobbering in the\n"
    "// presence of ES2015 semantics, which read RegExp.prototype.exec.\n"
    "saveBuiltin(RegExp,\n"
    "            ['exec']);\n"
    "saveBuiltin(Error,\n"
    "            [],\n"
    "            ['captureStackTrace']);\n"
    "saveBuiltin(Promise,\n"
    "            ['then', 'catch']);\n"
    "\n"
    "// JSON is trickier because extensions can override toJSON in\n"
    "// incompatible ways, and we need to prevent that.\n"
    "var builtinTypes = [\n"
    "  Object, Function, Array, String, Boolean, Number, Date, RegExp\n"
    "];\n"
    "var builtinToJSONs = builtinTypes.map(function(t) {\n"
    "  return t.toJSON;\n"
    "});\n"
    "var builtinArray = Array;\n"
    "var builtinJSONStringify = JSON.stringify;\n"
    "Save('JSON', {\n"
    "  parse: JSON.parse,\n"
    "  stringify: function(obj) {\n"
    "    var savedToJSONs = new builtinArray(builtinTypes.length);\n"
    "    try {\n"
    "      for (var i = 0; i < builtinTypes.length; ++i) {\n"
    "        try {\n"
    "          if (builtinTypes[i].prototype.toJSON !==\n"
    "              builtinToJSONs[i]) {\n"
    "            savedToJSONs[i] = builtinTypes[i].prototype.toJSON;\n"
    "            builtinTypes[i].prototype.toJSON = builtinToJSONs[i];\n"
    "          }\n"
    "        } catch (e) {}\n"
    "      }\n"
    "    } catch (e) {}\n"
    "    try {\n"
    "      return builtinJSONStringify(obj);\n"
    "    } finally {\n"
    "      for (var i = 0; i < builtinTypes.length; ++i) {\n"
    "        try {\n"
    "          if (i in savedToJSONs)\n"
    "            builtinTypes[i].prototype.toJSON = savedToJSONs[i];\n"
    "        } catch (e) {}\n"
    "      }\n"
    "    }\n"
    "  }\n"
    "});\n"
    "\n"
    "}());\n";

v8::Local<v8::Private> MakeKey(const char* name, v8::Isolate* isolate) {
  return v8::Private::ForApi(
      isolate, v8_helpers::ToV8StringUnsafe(
                   isolate, base::StringPrintf("%s::%s", kClassName, name)));
}

void SaveImpl(const char* name,
              v8::Local<v8::Value> value,
              v8::Local<v8::Context> context) {
  CHECK(!value.IsEmpty() && value->IsObject()) << name;
  context->Global()
      ->SetPrivate(context, MakeKey(name, context->GetIsolate()), value)
      .FromJust();
}

v8::Local<v8::Object> Load(const char* name, v8::Local<v8::Context> context) {
  v8::Local<v8::Value> value =
      context->Global()
          ->GetPrivate(context, MakeKey(name, context->GetIsolate()))
          .ToLocalChecked();
  CHECK(value->IsObject()) << name;
  return v8::Local<v8::Object>::Cast(value);
}

class ExtensionImpl : public v8::Extension {
 public:
  ExtensionImpl() : v8::Extension(kClassName, kScript) {}

 private:
  v8::Local<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate,
      v8::Local<v8::String> name) override {
    if (name->StringEquals(v8_helpers::ToV8StringUnsafe(isolate, "Apply")))
      return v8::FunctionTemplate::New(isolate, Apply);
    if (name->StringEquals(v8_helpers::ToV8StringUnsafe(isolate, "Save")))
      return v8::FunctionTemplate::New(isolate, Save);
    NOTREACHED_IN_MIGRATION() << *v8::String::Utf8Value(isolate, name);
    return v8::Local<v8::FunctionTemplate>();
  }

  static void Apply(const v8::FunctionCallbackInfo<v8::Value>& info) {
    CHECK(info.Length() == 5 && info[0]->IsFunction() &&  // function
          // info[1] could be an object or a string
          info[2]->IsObject() &&  // args
          info[3]->IsInt32() &&   // first_arg_index
          info[4]->IsInt32());    // args_length
    v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();
    v8::MicrotasksScope microtasks(info.GetIsolate(),
                                   context->GetMicrotaskQueue(),
                                   v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Local<v8::Function> function = info[0].As<v8::Function>();
    v8::Local<v8::Object> recv;
    if (info[1]->IsObject()) {
      recv = v8::Local<v8::Object>::Cast(info[1]);
    } else if (info[1]->IsString()) {
      recv = v8::StringObject::New(info.GetIsolate(),
                                   v8::Local<v8::String>::Cast(info[1]))
                 .As<v8::Object>();
    } else {
      info.GetIsolate()->ThrowException(
          v8::Exception::TypeError(v8_helpers::ToV8StringUnsafe(
              info.GetIsolate(),
              "The first argument is the receiver and must be an object")));
      return;
    }
    v8::Local<v8::Object> args = v8::Local<v8::Object>::Cast(info[2]);
    int first_arg_index = info[3].As<v8::Int32>()->Value();
    int args_length = info[4].As<v8::Int32>()->Value();

    int argc = args_length - first_arg_index;
    std::unique_ptr<v8::Local<v8::Value>[]> argv(
        new v8::Local<v8::Value>[argc]);
    for (int i = 0; i < argc; ++i) {
      CHECK(v8_helpers::IsTrue(args->Has(context, i + first_arg_index)));
      // Getting a property value could throw an exception.
      if (!v8_helpers::GetProperty(context, args, i + first_arg_index,
                                   &argv[i]))
        return;
    }

    v8::Local<v8::Value> return_value;
    if (function->Call(context, recv, argc, argv.get()).ToLocal(&return_value))
      info.GetReturnValue().Set(return_value);
  }

  static void Save(const v8::FunctionCallbackInfo<v8::Value>& info) {
    CHECK(info.Length() == 2 && info[0]->IsString() && info[1]->IsObject());
    SaveImpl(*v8::String::Utf8Value(info.GetIsolate(), info[0]), info[1],
             info.GetIsolate()->GetCurrentContext());
  }
};

}  // namespace

// static
std::unique_ptr<v8::Extension> SafeBuiltins::CreateV8Extension() {
  return std::make_unique<ExtensionImpl>();
}

SafeBuiltins::SafeBuiltins(ScriptContext* context) : context_(context) {}

SafeBuiltins::~SafeBuiltins() = default;

v8::Local<v8::Object> SafeBuiltins::GetArray() const {
  return Load("Array", context_->v8_context());
}

v8::Local<v8::Object> SafeBuiltins::GetFunction() const {
  return Load("Function", context_->v8_context());
}

v8::Local<v8::Object> SafeBuiltins::GetJSON() const {
  return Load("JSON", context_->v8_context());
}

v8::Local<v8::Object> SafeBuiltins::GetObjekt() const {
  return Load("Object", context_->v8_context());
}

v8::Local<v8::Object> SafeBuiltins::GetRegExp() const {
  return Load("RegExp", context_->v8_context());
}

v8::Local<v8::Object> SafeBuiltins::GetString() const {
  return Load("String", context_->v8_context());
}

v8::Local<v8::Object> SafeBuiltins::GetError() const {
  return Load("Error", context_->v8_context());
}

v8::Local<v8::Object> SafeBuiltins::GetPromise() const {
  return Load("Promise", context_->v8_context());
}

}  //  namespace extensions
