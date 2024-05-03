// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <iostream>

#include "base/memory/raw_ptr.h"
#include "javascript_parser.pb.h"  // from out/gen
#include "testing/libfuzzer/fuzzers/javascript_parser_proto_to_string.h"
#include "testing/libfuzzer/libfuzzer_exports.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

#include "v8/include/libplatform/libplatform.h"
#include "v8/include/v8.h"

// Silence logging from the protobuf library.
protobuf_mutator::protobuf::LogSilencer log_silencer;

// Encapsulates process-wide v8 state, initialized once per fuzzing session.
class Environment {
 public:
  // Returns the singleton v8 environment.
  static Environment& Get() {
    // Defining this as a static function variable instead of a global avoids
    // LSAN complaining about a leak.
    // See also: https://llvm.org/docs/LibFuzzer.html#startup-initialization
    static Environment environment;
    return environment;
  }

  v8::Isolate* isolate() { return isolate_; }

 private:
  Environment() {
    platform_ = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform_.get());
    v8::V8::Initialize();

    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator =
        v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    isolate_ = v8::Isolate::New(create_params);
  }

  std::unique_ptr<v8::Platform> platform_;
  raw_ptr<v8::Isolate> isolate_ = nullptr;
};

std::string protobuf_to_string(
    const javascript_parser_proto_fuzzer::Source& source_protobuf) {
  std::string source;
  for (const auto& token : source_protobuf.tokens()) {
    source += token_to_string(token, 0) + std::string(" ");
  }
  return source;
}

extern "C" int
LLVMFuzzerInitialize(int* argc, char*** argv) {
  v8::V8::InitializeICUDefaultLocation((*argv)[0]);
  v8::V8::InitializeExternalStartupData((*argv)[0]);
  v8::V8::SetFlagsFromCommandLine(argc, *argv, true);
  return 0;
}

DEFINE_BINARY_PROTO_FUZZER(
    const javascript_parser_proto_fuzzer::Source& source_protobuf) {
  v8::Isolate* isolate = Environment::Get().isolate();

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  std::string source_string = protobuf_to_string(source_protobuf);

  if (getenv("LPM_DUMP_NATIVE_INPUT")) {
    std::cout << source_string << std::endl;
    std::cout << "module: " << source_protobuf.is_module() << std::endl;
  }
  v8::Local<v8::String> source_v8_string =
      v8::String::NewFromUtf8(isolate, source_string.c_str(),
                              v8::NewStringType::kNormal)
          .ToLocalChecked();

  {
    v8::TryCatch try_catch(isolate);

    if (source_protobuf.is_module()) {
      v8::Local<v8::String> name =
          v8::String::NewFromUtf8(isolate, "module.js",
                                  v8::NewStringType::kNormal)
              .ToLocalChecked();

      v8::ScriptOrigin origin(name, 0, 0, false, -1, v8::Local<v8::Value>(),
                              false, false, true);
      v8::ScriptCompiler::Source source(source_v8_string, origin);
      v8::MaybeLocal<v8::Module> module =
          v8::ScriptCompiler::CompileModule(isolate, &source);
      // TODO(marja): Figure out a more elegant way to silence the warning.
      module.IsEmpty();
    } else {
      v8::MaybeLocal<v8::Script> script =
          v8::Script::Compile(context, source_v8_string);
      // TODO(marja): Figure out a more elegant way to silence the warning.
      script.IsEmpty();
    }

    // TODO(crbug.com/40545334): run the code once we find a way to avoid
    // endless loops.
  }
}
