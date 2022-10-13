// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

MLGraph::MLGraph(MLContext* context) : ml_context_(context) {}

MLGraph::~MLGraph() = default;

void MLGraph::Trace(Visitor* visitor) const {
  visitor->Trace(ml_context_);
  ScriptWrappable::Trace(visitor);
}

const HashMap<String, MLGraph::ResourceInfo>& MLGraph::GetInputResourcesInfo()
    const {
  DCHECK(resources_info_initialized_);
  return input_resources_info_;
}

const HashMap<String, MLGraph::ResourceInfo>& MLGraph::GetOutputResourcesInfo()
    const {
  DCHECK(resources_info_initialized_);
  return output_resources_info_;
}

void MLGraph::BuildAsync(const MLNamedOperands& named_outputs,
                         ScriptPromiseResolver* resolver) {
  String error_message;
  if (!ValidateAndInitializeResourcesInfo(named_outputs, error_message)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataError, error_message));
    return;
  }
  BuildAsyncImpl(named_outputs, resolver);
}

bool MLGraph::ValidateAndInitializeResourcesInfo(
    const MLNamedOperands& named_outputs,
    String& error_message) {
  DCHECK(!resources_info_initialized_);

  // The outputs should not be empty.
  if (named_outputs.empty()) {
    error_message = "At least one output needs to be provided.";
    return false;
  }

  // The queue and visited set of operators that help implement the
  // breadth-first graph traversal:
  // https://en.wikipedia.org/wiki/Breadth-first_search
  Deque<Member<const MLOperator>> operators_queue;
  HashSet<Member<const MLOperator>> visited_operators;

  // Validate the named outputs, setup corresponding output resource info and
  // initialize the queue and visited set with their dependent operators.
  for (const auto& output : named_outputs) {
    const auto& name = output.first;
    const auto& operand = output.second;
    // Validate whether it is an output operand.
    if (operand->Kind() != MLOperand::OperandKind::kOutput) {
      error_message = String::Format(
          "The operand with name \"%s\" is not an output operand.",
          name.Utf8().c_str());
      return false;
    }
    // Setup resource info for this output operand.
    output_resources_info_.insert(
        name, ResourceInfo({.type = operand->Type(),
                            .byte_length = operand->ByteLength()}));
    // Mark its dependent operator is visited.
    visited_operators.insert(operand->Operator());
    // Enqueue its dependent operator.
    operators_queue.push_back(operand->Operator());
  }

  while (operators_queue.size() > 0) {
    // If the queue is not empty, dequeue an operator from the queue.
    const auto& current_operator = operators_queue.front();
    operators_queue.pop_front();
    // Enumerate the current operator's input operands.
    for (const auto& operand : current_operator->Inputs()) {
      switch (operand->Kind()) {
        case MLOperand::OperandKind::kOutput:
          DCHECK(operand->Operator());
          // If the operand is an output operand and its dependent operator is
          // not visited, mark the dependent operator is visited and enqueue it.
          if (!visited_operators.Contains(operand->Operator())) {
            visited_operators.insert(operand->Operator());
            operators_queue.push_back(operand->Operator());
          }
          break;
        case MLOperand::OperandKind::kInput:
          // If the operand is an input operand, validate whether its name is
          // unique.
          if (input_resources_info_.Contains(operand->Name())) {
            error_message =
                String::Format("The input name \"%s\" is duplicated.",
                               operand->Name().Utf8().c_str());
            return false;
          }
          // Setup resource info for this input operand.
          input_resources_info_.insert(
              operand->Name(),
              ResourceInfo({.type = operand->Type(),
                            .byte_length = operand->ByteLength()}));
          break;
        case MLOperand::OperandKind::kConstant:
          // If the operand is a constant operand, there is no check needed.
          break;
      }
    }
  }
  resources_info_initialized_ = true;
  return true;
}

}  // namespace blink
