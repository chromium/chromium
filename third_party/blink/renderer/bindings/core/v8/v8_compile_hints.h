// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_COMPILE_HINTS_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_COMPILE_HINTS_H_

#include "third_party/blink/renderer/bindings/buildflags.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

#if BUILDFLAG(ENABLE_V8_COMPILE_HINTS)

#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

class ExecutionContext;
class Frame;
class Page;
class ScriptState;

/*
V8CrowdsourcedCompileHintsProducer gathers data about which JavaScript functions
are compiled and uploads it to UKM. On the server side, we construct a model
from this data and deliver it back to the users via OptimizationGuide (not yet
implemented).
*/
class V8CrowdsourcedCompileHintsProducer
    : public GarbageCollected<V8CrowdsourcedCompileHintsProducer> {
 public:
  explicit V8CrowdsourcedCompileHintsProducer(Page* page);

  V8CrowdsourcedCompileHintsProducer(
      const V8CrowdsourcedCompileHintsProducer&) = delete;
  V8CrowdsourcedCompileHintsProducer& operator=(
      const V8CrowdsourcedCompileHintsProducer&) = delete;

  // Notifies V8CrowdsourcedCompileHintsProducer of the existence of `script`
  void RecordScript(Frame* frame,
                    ExecutionContext* execution_context,
                    const v8::Local<v8::Script> script,
                    ScriptState* script_state);

  void GenerateData();

  void Trace(Visitor* visitor) const;

 private:
  void ClearData();
  bool SendDataToUkm();
  static void AddNoise(unsigned* data);

  WTF::Vector<v8::Global<v8::Script>> scripts_;
  WTF::Vector<uint32_t> script_name_hashes_;

  enum class State {
    kInitial,

    // We've tried once to send the data to UKM (but we didn't necessarily send
    // it successfully; e.g., because of throttling or because we didn't have
    // enough data).
    kDataGenerationFinished,
  };
  State state_ = State::kInitial;

  // Limit the data collection to happen only once per process (because the data
  // is so large). Not the same as the kDataGenerationFinished state, since we
  // might skip the data generation for one page, but still want to try whether
  // we get enough data from another page. Use std::atomic to be future proof
  // in case we start generating compile hints from Workers.
  static std::atomic<bool> data_generated_for_this_process_;

  Member<Page> page_;
};

}  // namespace blink

#else

namespace blink {

class Page;

// A minimal implementation for platforms which don't enable compile hints.
class V8CrowdsourcedCompileHintsProducer
    : public GarbageCollected<V8CrowdsourcedCompileHintsProducer> {
 public:
  explicit V8CrowdsourcedCompileHintsProducer(Page* page) {}

  V8CrowdsourcedCompileHintsProducer(
      const V8CrowdsourcedCompileHintsProducer&) = delete;
  V8CrowdsourcedCompileHintsProducer& operator=(
      const V8CrowdsourcedCompileHintsProducer&) = delete;

  void GenerateData() {}

  void Trace(Visitor* visitor) const {}
};

}  // namespace blink

#endif  // BUILDFLAG(ENABLE_V8_COMPILE_HINTS)

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_COMPILE_HINTS_H_
