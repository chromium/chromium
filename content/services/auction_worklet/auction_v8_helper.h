// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_V8_HELPER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_V8_HELPER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "gin/public/isolate_holder.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "url/gurl.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-persistent-handle.h"

namespace v8 {
class UnboundScript;
class WasmModuleObject;
}  // namespace v8

namespace v8_inspector {
class V8Inspector;
}  // namespace v8_inspector

namespace auction_worklet {

class AuctionV8DevToolsAgent;
class DebugCommandQueue;

// Helper for Javascript operations. Owns a V8 isolate, and manages operations
// on it. Must be deleted after all V8 objects created using its isolate. It
// facilitates creating objects from JSON and running scripts in isolated
// contexts.
//
// Currently, multiple AuctionV8Helpers can be in use at once, each will have
// its own V8 isolate.  All AuctionV8Helpers are assumed to be created on the
// same thread (V8 startup is done only once per process, and not behind a
// lock).  After creation, all public operations on the helper must be done on
// the thread represented by the `v8_runner` argument to Create(). It's the
// caller's responsibility to ensure that all other methods are used from the v8
// runner.
class CONTENT_EXPORT AuctionV8Helper
    : public base::RefCountedDeleteOnSequence<AuctionV8Helper> {
 public:
  // Timeout for script execution.
  static const base::TimeDelta kScriptTimeout;

  // Status of a computation that may timeout.
  enum class Result { kSuccess, kFailure, kTimeout };

  // Helper class to set up v8 scopes to use Isolate. All methods expect a
  // FullIsolateScope to be have been created on the current thread, and a
  // context to be entered.
  class CONTENT_EXPORT FullIsolateScope {
   public:
    explicit FullIsolateScope(AuctionV8Helper* v8_helper);
    explicit FullIsolateScope(const FullIsolateScope&) = delete;
    FullIsolateScope& operator=(const FullIsolateScope&) = delete;
    ~FullIsolateScope();

   private:
    const v8::Isolate::Scope isolate_scope_;
    const v8::HandleScope handle_scope_;
  };

  // A wrapper for identifiers used to associate V8 context's with debugging
  // primitives.  Passed to methods like Compile and RunScript. If one is
  // created, AbortDebuggerPauses() must be called before its destruction.
  //
  // This class is thread-safe, except SetResumeCallback must be used from V8
  // thread.
  class CONTENT_EXPORT DebugId : public base::RefCountedThreadSafe<DebugId> {
   public:
    explicit DebugId(AuctionV8Helper* v8_helper);

    // Returns V8 context group ID associated with this debug id.
    int context_group_id() const { return context_group_id_; }

    // Sets the callback to use to resume a worklet that's paused on startup.
    // Must be called from the V8 thread.
    //
    // `resume_callback` will be invoked on the V8 thread; and should probably
    // be bound to a a WeakPtr, since the invocation is ultimately via debugger
    // mojo pipes, making its timing hard to relate to worklet lifetime.
    void SetResumeCallback(base::OnceClosure resume_callback);

    // If the JS thread is currently within AuctionV8Helper::RunScript() running
    // code with this debug id, and the execution has been paused by the
    // debugger, aborts the execution.
    //
    // Always prevents further debugger pauses of code associated with this
    // debug id.
    //
    // This may be called from any thread, but note that posting this to the V8
    // thread is unlikely to work, since this method is in particular useful for
    // the cases where the V8 thread is blocked.
    void AbortDebuggerPauses();

   private:
    friend class base::RefCountedThreadSafe<DebugId>;
    ~DebugId();

    const scoped_refptr<AuctionV8Helper> v8_helper_;
    const int context_group_id_;
  };

  // Representation for results of serialization via SerializeValue().
  // Helps with the memory management. Movable but not copyable.
  class CONTENT_EXPORT SerializedValue {
   public:
    SerializedValue();
    SerializedValue(const SerializedValue&) = delete;
    SerializedValue(SerializedValue&& other);
    ~SerializedValue();

    SerializedValue& operator=(const SerializedValue&) = delete;
    SerializedValue& operator=(SerializedValue&&);

    bool IsOK() const { return buffer_; }

   private:
    friend class AuctionV8Helper;
    raw_ptr<uint8_t> buffer_;
    size_t size_;
  };

  // Represents a time limit that's shared by a group of operations (so if it's
  // 50ms and first takes 30ms and second tries to take 25ms, it will be
  // interrupted at around 20ms).
  class CONTENT_EXPORT TimeLimit {
   public:
    virtual ~TimeLimit();

    // Resumes the timer if it's not already running. Returns true if the timer
    // was resumed, false if it was already running.
    //
    // You do not need to call this directly if you're using `RunScript` or
    // `CallFunction`.
    virtual bool Resume() = 0;

    // Pauses the timer (must be running). You do not need to
    // call it directly if you're using `RunScript` or `CallFunction`.
    virtual void Pause() = 0;

    AuctionV8Helper* v8_helper() { return v8_helper_; }

   protected:
    explicit TimeLimit(AuctionV8Helper* v8_helper) : v8_helper_(v8_helper) {}

   private:
    const raw_ptr<AuctionV8Helper> v8_helper_;
  };

  // Helper that calls Resume()/Pause() if given a non-nullptr TimeLimit.
  //
  // v8::TryCatch::HasTerminated() can help detect the timeouts.
  //
  // This is safe to use recursively.
  class CONTENT_EXPORT TimeLimitScope {
   public:
    explicit TimeLimitScope(TimeLimit* script_timeout);
    ~TimeLimitScope();

    bool has_time_limit() const { return script_timeout_; }

   private:
    raw_ptr<TimeLimit> script_timeout_;

    bool resumed_ = false;
  };

  explicit AuctionV8Helper(const AuctionV8Helper&) = delete;
  AuctionV8Helper& operator=(const AuctionV8Helper&) = delete;

  static scoped_refptr<AuctionV8Helper> Create(
      scoped_refptr<base::SingleThreadTaskRunner> v8_runner,
      bool init_v8 = true);
  static scoped_refptr<base::SingleThreadTaskRunner> CreateTaskRunner();

  scoped_refptr<base::SequencedTaskRunner> v8_runner() const {
    return v8_runner_;
  }

  // Note: `callback` will be called on `v8_runner()`. This method may be called
  // on the creation thread if done before any non-initialization work on v8
  // thread begins.
  void SetDestroyedCallback(base::OnceClosure callback);

  v8::Isolate* isolate() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return isolate_holder_->isolate();
  }

  // Context that can be used for persistent items that can then be used in
  // other contexts - compiling functions, creating objects, etc.
  v8::Local<v8::Context> scratch_context() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return scratch_context_.Get(isolate());
  }

  // Create a v8::Context. The one thing this does that v8::Context::New() does
  // not is remove access to the Date object.
  v8::Local<v8::Context> CreateContext(
      v8::Local<v8::ObjectTemplate> global_template =
          v8::Local<v8::ObjectTemplate>());

  // Creates a v8::String from an ASCII string literal, which should never fail.
  v8::Local<v8::String> CreateStringFromLiteral(const char* ascii_string);

  // Attempts to create a v8::String from a UTF-8 string. Returns empty string
  // if input is not UTF-8.
  v8::MaybeLocal<v8::String> CreateUtf8String(std::string_view utf8_string);

  // The passed in JSON must be a valid UTF-8 JSON string.
  v8::MaybeLocal<v8::Value> CreateValueFromJson(v8::Local<v8::Context> context,
                                                std::string_view utf8_json);

  // Convenience wrappers around the above Create* methods. Attempt to create
  // the corresponding value type and append it to the passed in argument
  // vector. Useful for assembling arguments to a Javascript function. Return
  // false on failure.
  [[nodiscard]] bool AppendUtf8StringValue(std::string_view utf8_string,
                                           v8::LocalVector<v8::Value>* args);
  [[nodiscard]] bool AppendJsonValue(v8::Local<v8::Context> context,
                                     std::string_view utf8_json,
                                     v8::LocalVector<v8::Value>* args);

  // Convenience wrapper that adds the specified value into the provided Object.
  [[nodiscard]] bool InsertValue(std::string_view key,
                                 v8::Local<v8::Value> value,
                                 v8::Local<v8::Object> object);

  // Convenience wrapper that creates an Object by parsing `utf8_json` as JSON
  // and then inserts it into the provided Object.
  [[nodiscard]] bool InsertJsonValue(v8::Local<v8::Context> context,
                                     std::string_view key,
                                     std::string_view utf8_json,
                                     v8::Local<v8::Object> object);

  // Attempts to convert |value| to JSON and write it to |out|.
  Result ExtractJson(v8::Local<v8::Context> context,
                     v8::Local<v8::Value> value,
                     TimeLimit* script_timeout,
                     std::string* out);

  // Serializes |value| via v8::ValueSerializer and returns it. This is faster
  // than JSON. The return value can be used (and deserialized) in any context,
  // and can be freed on any thread (though some malloc implementations would
  // prefer if it were to be freed on v8 thread).
  SerializedValue Serialize(v8::Local<v8::Context> context,
                            v8::Local<v8::Value> value);

  // Deserializes `value` via v8::ValueDeserializer in `context`.
  v8::MaybeLocal<v8::Value> Deserialize(
      v8::Local<v8::Context> context,
      const SerializedValue& serialized_value);

  // Compiles the provided script. Despite not being bound to a context, there
  // still must be an active context for this method to be invoked. In case of
  // an error sets `error_out`.
  v8::MaybeLocal<v8::UnboundScript> Compile(
      const std::string& src,
      const GURL& src_url,
      const DebugId* debug_id,
      std::optional<std::string>& error_out);

  // Compiles the provided WASM module from bytecode. A context must be active
  // for this method to be invoked, and the object would be created for it (but
  // may be cloned efficiently for other contexts via CloneWasmModule). In case
  // of an error sets `error_out`.
  //
  // Note that since the returned object is a JS Object, so to properly isolate
  // different executions it should not be used directly but rather fresh copies
  // should be made via CloneWasmModule.
  v8::MaybeLocal<v8::WasmModuleObject> CompileWasm(
      const std::string& payload,
      const GURL& src_url,
      const DebugId* debug_id,
      std::optional<std::string>& error_out);

  // Creates a fresh object describing the same WASM module as `in`, which must
  // not be empty. Can return an empty handle on an error.
  //
  // An execution context must be active, and the object will be created for it.
  v8::MaybeLocal<v8::WasmModuleObject> CloneWasmModule(
      v8::Local<v8::WasmModuleObject> in);

  // Creates a time limiter for a group of operations. Note that it registers
  // itself with `this` and must not outlive it, and there shouldn't be more
  // than one at a time per AuctionV8Helper.
  //
  // If `script_timeout` has no value, kScriptTimeout will be used as the
  // default timeout.
  std::unique_ptr<TimeLimit> CreateTimeLimit(
      std::optional<base::TimeDelta> script_timeout);

  // Returns the currently active time limit, if any.
  TimeLimit* GetTimeLimit();

  // Binds a script and runs it in the passed in context, returning whether it
  // succeeded.
  //
  // If `debug_id` is not nullptr, and a debugger connection has been
  // instantiated, will notify debugger of `context`.
  //
  // Assumes passed in context is the active context. Passed in context must be
  // using the Helper's isolate.
  //
  // If `script_timeout` is set, it will be used as a time limit for this
  // operation. (If nullptr, the script may take an arbitrary amount of time or
  // might fail to terminate).
  //
  // In case of an error appends it to `error_out`.
  Result RunScript(v8::Local<v8::Context> context,
                   v8::Local<v8::UnboundScript> script,
                   const DebugId* debug_id,
                   TimeLimit* script_timeout,
                   std::vector<std::string>& error_out);

  // Calls a bound function (by name) attached to the global context in the
  // passed in context and returns the value returned by the function. Note that
  // the returned value could include references to objects or functions
  // contained within the context, so is likely not safe to use in other
  // contexts without sanitization.
  //
  // `script_name` is the name of the script for debugging. Can be found by
  // calling `FormatScriptName` on the `script` passed to `RunScript()`.
  //
  // `function_name` will be called passing in `args` as arguments.
  //
  // If `debug_id` is not nullptr, and a debugger connection has been
  // instantiated, will notify debugger of `context`.
  //
  // Assumes passed in context is the active context. Passed in context must be
  // using the Helper's isolate.
  //
  // If `script_timeout` is set, it will be used as a time limit for this
  // operation. (If nullptr, the function may take an arbitrary amount of time
  // or might fail to terminate).
  //
  // Returns whether successful or not. `value_out` will be non-empty (and set
  // to the return value) if and only if successful.
  //
  // In case of an error appends it to `error_out`.
  Result CallFunction(v8::Local<v8::Context> context,
                      const DebugId* debug_id,
                      const std::string& script_name,
                      std::string_view function_name,
                      base::span<v8::Local<v8::Value>> args,
                      TimeLimit* script_timeout,
                      v8::MaybeLocal<v8::Value>& value_out,
                      std::vector<std::string>& error_out);

  // If any debugging session targeting `debug_id` has set an active
  // DOM instrumentation breakpoint `name`, asks for v8 to do a debugger pause
  // on the next statement.
  //
  // Expected to be run before a corresponding RunScript.
  void MaybeTriggerInstrumentationBreakpoint(const DebugId& debug_id,
                                             const std::string& name);

  void set_script_timeout_for_testing(base::TimeDelta script_timeout);

  // Invokes the registered resume callback for given ID. Does nothing if it
  // was already invoked.
  void Resume(int context_group_id);

  // Overrides what ID will be remembered as last returned to help check the
  // allocation algorithm.
  void SetLastContextGroupIdForTesting(int new_last_id);

  // Calls Resume on all registered context group IDs.
  void ResumeAllForTesting();

  // Establishes a debugger connection, initializing debugging objects if
  // needed, and associating the connection with the given `debug_id`.
  //
  // The debugger Mojo objects will primarily live on the v8 thread, but
  // `mojo_sequence` will be used for a secondary communication channel in case
  // the v8 thread is blocked. It must be distinct from v8_runner(). Only the
  // value passed in for `mojo_sequence` the first time this method is called
  // will be used.
  void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent,
      scoped_refptr<base::SequencedTaskRunner> mojo_sequence,
      const DebugId& debug_id);

  // Returns the v8 inspector if one has been set. null if ConnectDevToolsAgent
  // (or SetV8InspectorForTesting) hasn't been called.
  v8_inspector::V8Inspector* inspector();

  void SetV8InspectorForTesting(
      std::unique_ptr<v8_inspector::V8Inspector> v8_inspector);

  // Temporarily disables (and re-enables) script timeout for the currently
  // running script. Total time elapsed when not paused will be kept track of.
  //
  // Must be called when within RunScript() only.
  void PauseTimeoutTimer();
  void ResumeTimeoutTimer();

  // Returns the sequence where the timeout timer runs.
  // This may be called on any thread.
  scoped_refptr<base::SequencedTaskRunner> GetTimeoutTimerRunnerForTesting();

  // Helper for formatting script name for debug messages.
  std::string FormatScriptName(v8::Local<v8::UnboundScript> script);

  static std::string FormatExceptionMessage(v8::Local<v8::Context> context,
                                            v8::Local<v8::Message> message);

 private:
  friend class base::RefCountedDeleteOnSequence<AuctionV8Helper>;
  friend class base::DeleteHelper<AuctionV8Helper>;
  class ScriptTimeoutHelper;

  AuctionV8Helper(scoped_refptr<base::SingleThreadTaskRunner> v8_runner,
                  bool init_v8);
  ~AuctionV8Helper();

  void CreateIsolate();

  // These methods are used by DebugId, and except SetResumeCallback can be
  // called from any thread.
  int AllocContextGroupId();
  void SetResumeCallback(int context_group_id,
                         base::OnceClosure resume_callback);
  void AbortDebuggerPauses(int context_group_id);
  void FreeContextGroupId(int context_group_id);

  static std::string FormatValue(v8::Isolate* isolate,
                                 v8::Local<v8::Value> val);

  scoped_refptr<base::SequencedTaskRunner> v8_runner_;
  scoped_refptr<base::SequencedTaskRunner> timer_task_runner_;

  // This needs to be invoked after ~IsolateHolder to make sure that V8 is
  // really shut down.
  base::ScopedClosureRunner destroyed_callback_run_;

  std::unique_ptr<gin::IsolateHolder> isolate_holder_
      GUARDED_BY_CONTEXT(sequence_checker_);
  v8::Global<v8::Context> scratch_context_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // Script timeout. Can be changed for testing.
  base::TimeDelta script_timeout_ GUARDED_BY_CONTEXT(sequence_checker_) =
      kScriptTimeout;

  raw_ptr<ScriptTimeoutHelper> timeout_helper_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  base::Lock context_groups_lock_;
  int last_context_group_id_ GUARDED_BY(context_groups_lock_) = 0;

  // This is keyed by group IDs, and is used to keep track of what's valid.
  std::map<int, base::OnceClosure> resume_callbacks_
      GUARDED_BY(context_groups_lock_);

  scoped_refptr<DebugCommandQueue> debug_command_queue_;

  // Destruction order between `devtools_agent_` and `v8_inspector_` is
  // relevant; see also comment in ~AuctionV8Helper().
  std::unique_ptr<AuctionV8DevToolsAgent> devtools_agent_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<v8_inspector::V8Inspector> v8_inspector_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_V8_HELPER_H_
