// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_COMPILE_HINTS_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_COMPILE_HINTS_H_

#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

class ExecutionContext;
class Frame;
class ScriptState;

class V8CompileHints {
 public:
  // Notifies V8CompileHints of the existence of `script`. Also schedules data
  // generation to happen later.
  void RecordScript(Frame* frame,
                    ExecutionContext* execution_context,
                    const v8::Local<v8::Script> script,
                    ScriptState* script_state);

  void GenerateData(ExecutionContext* execution_context);

 private:
  void ClearData();
  void ScheduleDataGenerationIfNeeded(Frame* frame,
                                      ExecutionContext* execution_context);
  bool SendDataToUkm(ExecutionContext* execution_context);
  static void AddNoise(unsigned* data);

  WTF::Vector<v8::Global<v8::Script>> scripts_;
  WTF::Vector<uint32_t> script_name_hashes_;

  enum class State {
    kInitial,

    // Task fro data generation has been scheduled.
    kDataGenerationScheduled,

    // Task for data generation has ran.
    kDataGenerationFinished
  };
  State state_ = State::kInitial;

  // Limit the data collection to happen only once per process (because the data
  // is so large). Not the same as the kDataGenerationFinished state, since we
  // might skip the data generation for one page, but still want to try whether
  // we get enough data from another page. Use std::atomic to be future proof
  // in case we start generating compile hints from Workers.
  static std::atomic<bool> data_generated_for_this_process_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_COMPILE_HINTS_H_
