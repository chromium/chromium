// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_v8_helper.h"

#include <memory>

#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "gin/array_buffer.h"
#include "gin/converter.h"
#include "gin/gin_features.h"
#include "gin/public/isolate_holder.h"
#include "gin/v8_initializer.h"
#include "v8/include/v8.h"

namespace auction_worklet {

namespace {

// Initialize V8 (and gin).
void InitV8() {
  // TODO(mmenke): All these calls touch global state, which seems rather unsafe
  // if the process is shared with anything else (e.g. --single-process mode, or
  // on Android?).  Is there some safer way to do this?
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  gin::V8Initializer::LoadV8Snapshot();
#endif

  // Each script is run once using its own isolate, so tune down V8 to use as
  // little memory as possible.
  static const char kOptimizeForSize[] = "--optimize_for_size";
  v8::V8::SetFlagsFromString(kOptimizeForSize, strlen(kOptimizeForSize));

  // Running v8 in jitless mode allows dynamic code to be disabled in the
  // process, and since each isolate is used only once, this may be best for
  // performance as well.
  static const char kJitless[] = "--jitless";
  v8::V8::SetFlagsFromString(kJitless, strlen(kJitless));

  // WebAssembly isn't encountered during resolution, so reduce the
  // potential attack surface.
  static const char kNoExposeWasm[] = "--no-expose-wasm";
  v8::V8::SetFlagsFromString(kNoExposeWasm, strlen(kNoExposeWasm));

  gin::IsolateHolder::Initialize(gin::IsolateHolder::kNonStrictMode,
                                 gin::ArrayBufferAllocator::SharedInstance());
}

// Utility class to timeout running a v8::Script or calling a v8::Function.
// Instantiate a ScriptTimeoutHelper, and it will terminate script if
// kScriptTimeout passes before it is destroyed.
//
// Creates a v8::SafeForTerminationScope(), so the caller doesn't have to.
class ScriptTimeoutHelper {
 public:
  ScriptTimeoutHelper(v8::Isolate* isolate, base::TimeDelta script_timeout)
      : termination_scope_(isolate),
        timer_task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})),
        off_thread_timer_(std::make_unique<OffThreadTimer>(timer_task_runner_,
                                                           isolate,
                                                           script_timeout)) {}

  ScriptTimeoutHelper(const ScriptTimeoutHelper&) = delete;
  ScriptTimeoutHelper& operator=(const ScriptTimeoutHelper&) = delete;

  ~ScriptTimeoutHelper() {
    off_thread_timer_->CancelTimer();
    timer_task_runner_->DeleteSoon(FROM_HERE, std::move(off_thread_timer_));
  }

 private:
  // Class to call TerminateExecution on an Isolate on a specified thread once
  // `AuctionV8Helper::kScriptTimeout` has passed. Create on the sequence the
  // Isolate is running scripts on, but must be destroyed on the task runner the
  // timer is run on.
  class OffThreadTimer {
   public:
    OffThreadTimer(scoped_refptr<base::SequencedTaskRunner> timer_task_runner,
                   v8::Isolate* isolate,
                   base::TimeDelta script_timeout)
        : isolate_(isolate) {
      timer_task_runner->PostTask(
          FROM_HERE, base::BindOnce(&OffThreadTimer::StartTimer,
                                    base::Unretained(this), script_timeout));
    }

    ~OffThreadTimer() { DCHECK(!isolate_); }

    // Must be called on the Isolate sequence before a task is posted to destroy
    // the OffThreadTimer on the timer sequence.
    void CancelTimer() {
      base::AutoLock autolock(lock_);
      // In the unlikely case AbortScript() was executed just after a script
      // completed, but before CancelTimer() was invoked, clear the exception.
      if (terminate_execution_called_)
        isolate_->CancelTerminateExecution();
      isolate_ = nullptr;
    }

   private:
    void StartTimer(base::TimeDelta script_timeout) {
      timer_.Start(
          FROM_HERE, script_timeout,
          base::BindOnce(&OffThreadTimer::AbortScript, base::Unretained(this)));
    }

    void AbortScript() {
      base::AutoLock autolock(lock_);
      if (!isolate_)
        return;
      terminate_execution_called_ = true;
      isolate_->TerminateExecution();
    }

    // Used solely on `timer_task_runner`.
    base::OneShotTimer timer_;

    base::Lock lock_;

    // Isolate to terminate execution of when time expires. Set to nullptr on
    // the Isolate thread before destruction, to avoid any teardown races with
    // script execution ending.
    v8::Isolate* isolate_ GUARDED_BY(lock_);

    bool terminate_execution_called_ GUARDED_BY(lock_) = false;
  };

  v8::Isolate::SafeForTerminationScope termination_scope_;

  scoped_refptr<base::SequencedTaskRunner> timer_task_runner_;

  std::unique_ptr<OffThreadTimer> off_thread_timer_;
};

}  // namespace

constexpr base::TimeDelta AuctionV8Helper::kScriptTimeout =
    base::TimeDelta::FromMilliseconds(50);

AuctionV8Helper::FullIsolateScope::FullIsolateScope(AuctionV8Helper* v8_helper)
    : locker_(v8_helper->isolate()),
      isolate_scope_(v8_helper->isolate()),
      handle_scope_(v8_helper->isolate()) {}

AuctionV8Helper::FullIsolateScope::~FullIsolateScope() = default;

AuctionV8Helper::AuctionV8Helper() {
  static int v8_initialized = false;
  if (!v8_initialized)
    InitV8();

  v8_initialized = true;

  // Now the initialization is completed, create an isolate.
  isolate_holder_ = std::make_unique<gin::IsolateHolder>(
      base::ThreadTaskRunnerHandle::Get(), gin::IsolateHolder::kUseLocker,
      gin::IsolateHolder::IsolateType::kUtility);
  FullIsolateScope v8_scope(this);
  scratch_context_.Reset(isolate(), CreateContext());
}

AuctionV8Helper::~AuctionV8Helper() = default;

v8::Local<v8::Context> AuctionV8Helper::CreateContext(
    v8::Handle<v8::ObjectTemplate> global_template) {
  v8::Local<v8::Context> context =
      v8::Context::New(isolate(), nullptr /* extensions */, global_template);
  auto result =
      context->Global()->Delete(context, CreateStringFromLiteral("Date"));

  v8::Local<v8::ObjectTemplate> console_emulation =
      console_.GetConsoleTemplate();
  v8::Local<v8::Object> console_obj;
  if (console_emulation->NewInstance(context).ToLocal(&console_obj)) {
    result = context->Global()->Set(context, CreateStringFromLiteral("console"),
                                    console_obj);
    DCHECK(!result.IsNothing());
  } else {
    DCHECK(false);
  }

  return context;
}

v8::Local<v8::String> AuctionV8Helper::CreateStringFromLiteral(
    const char* ascii_string) {
  DCHECK(base::IsStringASCII(ascii_string));
  return v8::String::NewFromUtf8(isolate(), ascii_string,
                                 v8::NewStringType::kNormal,
                                 strlen(ascii_string))
      .ToLocalChecked();
}

v8::MaybeLocal<v8::String> AuctionV8Helper::CreateUtf8String(
    base::StringPiece utf8_string) {
  if (!base::IsStringUTF8(utf8_string))
    return v8::MaybeLocal<v8::String>();
  return v8::String::NewFromUtf8(isolate(), utf8_string.data(),
                                 v8::NewStringType::kNormal,
                                 utf8_string.length());
}

v8::MaybeLocal<v8::Value> AuctionV8Helper::CreateValueFromJson(
    v8::Local<v8::Context> context,
    base::StringPiece utf8_json) {
  v8::Local<v8::String> v8_string;
  if (!CreateUtf8String(utf8_json).ToLocal(&v8_string))
    return v8::MaybeLocal<v8::Value>();
  return v8::JSON::Parse(context, v8_string);
}

bool AuctionV8Helper::AppendUtf8StringValue(
    base::StringPiece utf8_string,
    std::vector<v8::Local<v8::Value>>* args) {
  v8::Local<v8::String> value;
  if (!CreateUtf8String(utf8_string).ToLocal(&value))
    return false;
  args->push_back(value);
  return true;
}

bool AuctionV8Helper::AppendJsonValue(v8::Local<v8::Context> context,
                                      base::StringPiece utf8_json,
                                      std::vector<v8::Local<v8::Value>>* args) {
  v8::Local<v8::Value> value;
  if (!CreateValueFromJson(context, utf8_json).ToLocal(&value))
    return false;
  args->push_back(value);
  return true;
}

bool AuctionV8Helper::InsertValue(base::StringPiece key,
                                  v8::Local<v8::Value> value,
                                  v8::Local<v8::Object> object) {
  v8::Local<v8::String> v8_key;
  if (!CreateUtf8String(key).ToLocal(&v8_key))
    return false;
  v8::Maybe<bool> result =
      object->Set(isolate()->GetCurrentContext(), v8_key, value);
  return !result.IsNothing() && result.FromJust();
}

bool AuctionV8Helper::InsertJsonValue(v8::Local<v8::Context> context,
                                      base::StringPiece key,
                                      base::StringPiece utf8_json,
                                      v8::Local<v8::Object> object) {
  v8::Local<v8::Value> v8_value;
  return CreateValueFromJson(context, utf8_json).ToLocal(&v8_value) &&
         InsertValue(key, v8_value, object);
}

// Attempts to convert |value| to JSON and write it to |out|. Returns false on
// failure.
bool AuctionV8Helper::ExtractJson(v8::Local<v8::Context> context,
                                  v8::Local<v8::Value> value,
                                  std::string* out) {
  v8::MaybeLocal<v8::String> maybe_json = v8::JSON::Stringify(context, value);
  v8::Local<v8::String> json;
  if (!maybe_json.ToLocal(&json))
    return false;
  bool success = gin::ConvertFromV8(isolate(), json, out);
  if (!success)
    return false;
  // Stringify can return the string "undefined" for certain inputs, which is
  // not actually JSON. Treat those as failures.
  if (*out == "undefined") {
    out->clear();
    return false;
  }
  return true;
}

v8::MaybeLocal<v8::UnboundScript> AuctionV8Helper::Compile(
    const std::string& src,
    const GURL& src_url,
    absl::optional<std::string>& error_out) {
  v8::Isolate* v8_isolate = isolate();

  v8::MaybeLocal<v8::String> src_string = CreateUtf8String(src);
  v8::MaybeLocal<v8::String> origin_string = CreateUtf8String(src_url.spec());
  if (src_string.IsEmpty() || origin_string.IsEmpty())
    return v8::MaybeLocal<v8::UnboundScript>();

  // Compile script.
  v8::TryCatch try_catch(isolate());
  v8::ScriptCompiler::Source script_source(
      src_string.ToLocalChecked(),
      v8::ScriptOrigin(v8_isolate, origin_string.ToLocalChecked()));
  auto result = v8::ScriptCompiler::CompileUnboundScript(
      v8_isolate, &script_source, v8::ScriptCompiler::kNoCompileOptions,
      v8::ScriptCompiler::NoCacheReason::kNoCacheNoReason);
  if (try_catch.HasCaught()) {
    error_out = FormatExceptionMessage(v8_isolate->GetCurrentContext(),
                                       try_catch.Message());
  }
  return result;
}

v8::MaybeLocal<v8::Value> AuctionV8Helper::RunScript(
    v8::Local<v8::Context> context,
    v8::Local<v8::UnboundScript> script,
    base::StringPiece script_name,
    base::span<v8::Local<v8::Value>> args,
    std::vector<std::string>& error_out) {
  DCHECK_EQ(isolate(), context->GetIsolate());

  ScopedConsoleTarget direct_console(
      this, FormatValue(isolate(), script->GetScriptName()), &error_out);

  v8::Local<v8::String> v8_script_name;
  if (!CreateUtf8String(script_name).ToLocal(&v8_script_name))
    return v8::MaybeLocal<v8::Value>();

  v8::Local<v8::Script> local_script = script->BindToCurrentContext();

  // Run script.
  v8::TryCatch try_catch(isolate());
  ScriptTimeoutHelper timeout_helper(isolate(), script_timeout_);
  auto result = local_script->Run(context);

  if (try_catch.HasTerminated()) {
    error_out.push_back(
        base::StrCat({FormatValue(isolate(), script->GetScriptName()),
                      " top-level execution timed out."}));
    return v8::MaybeLocal<v8::Value>();
  }

  if (try_catch.HasCaught()) {
    error_out.push_back(FormatExceptionMessage(context, try_catch.Message()));
    return v8::MaybeLocal<v8::Value>();
  }

  if (result.IsEmpty())
    return v8::MaybeLocal<v8::Value>();

  v8::Local<v8::Value> function;
  if (!context->Global()->Get(context, v8_script_name).ToLocal(&function)) {
    error_out.push_back(
        base::StrCat({FormatValue(isolate(), script->GetScriptName()),
                      " function `", script_name, "` not found."}));
    return v8::MaybeLocal<v8::Value>();
  }

  if (!function->IsFunction()) {
    error_out.push_back(
        base::StrCat({FormatValue(isolate(), script->GetScriptName()), " `",
                      script_name, "` is not a function."}));
    return v8::MaybeLocal<v8::Value>();
  }

  v8::MaybeLocal<v8::Value> func_result = v8::Function::Cast(*function)->Call(
      context, context->Global(), args.size(), args.data());
  if (try_catch.HasTerminated()) {
    error_out.push_back(
        base::StrCat({FormatValue(isolate(), script->GetScriptName()),
                      " execution of `", script_name, "` timed out."}));
    return v8::MaybeLocal<v8::Value>();
  }
  if (try_catch.HasCaught()) {
    error_out.push_back(FormatExceptionMessage(context, try_catch.Message()));
    return v8::MaybeLocal<v8::Value>();
  }
  return func_result;
}

AuctionV8Helper::ScopedConsoleTarget::ScopedConsoleTarget(
    AuctionV8Helper* owner,
    const std::string& console_script_name,
    std::vector<std::string>* out)
    : owner_(owner) {
  DCHECK(!owner_->console_buffer_);
  DCHECK(owner_->console_script_name_.empty());
  owner_->console_buffer_ = out;
  owner_->console_script_name_ = console_script_name;
}

AuctionV8Helper::ScopedConsoleTarget::~ScopedConsoleTarget() {
  owner_->console_buffer_ = nullptr;
  owner_->console_script_name_ = std::string();
}

// static
std::string AuctionV8Helper::FormatExceptionMessage(
    v8::Local<v8::Context> context,
    v8::Local<v8::Message> message) {
  if (message.IsEmpty()) {
    return "Unknown exception.";
  } else {
    v8::Isolate* isolate = message->GetIsolate();
    int line_num;
    return base::StrCat(
        {FormatValue(isolate, message->GetScriptResourceName()),
         !context.IsEmpty() && message->GetLineNumber(context).To(&line_num)
             ? std::string(":") + base::NumberToString(line_num)
             : std::string(),
         " ", FormatValue(isolate, message->Get()), "."});
  }
}

// static
std::string AuctionV8Helper::FormatValue(v8::Isolate* isolate,
                                         v8::Local<v8::Value> val) {
  if (val.IsEmpty()) {
    return "\"\"";
  } else {
    v8::String::Utf8Value val_utf8(isolate, val);
    if (*val_utf8 == nullptr)
      return std::string();
    return std::string(*val_utf8, val_utf8.length());
  }
}

}  // namespace auction_worklet
